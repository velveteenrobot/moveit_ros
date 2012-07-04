/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2011, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include "trajectory_execution_manager/trajectory_execution_manager.h"
#include "/u/isucan/projects/moveit/moveit_ros/trajectory_execution_manager/test/test_moveit_controller_manager.h"

namespace trajectory_execution_manager
{

void TrajectoryExecutionManager::initialize(void)
{ 
  execution_complete_ = true;
  /*
  // load the controller manager plugin
  try
  {
    controller_manager_loader_.reset(new pluginlib::ClassLoader<moveit_controller_manager::MoveItControllerManager>("moveit_controller_manager", "moveit_controller_manager::MoveItControllerManager"));
  }
  catch(pluginlib::PluginlibException& ex)
  {
    ROS_FATAL_STREAM("Exception while creating controller manager plugin loader: " << ex.what());
    return;
  }

  std::string controller;
  if (!node_handle_.getParam("controller_manager", controller))
  {
    const std::vector<std::string> &classes = controller_manager_loader_->getDeclaredClasses();
    if (classes.size() == 1)
    {
      controller = classes[0];
      ROS_WARN("Parameter '~controller_manager' is not specified but only one matching plugin was found: '%s'. Using that one.", controller.c_str());
    }
    else
      ROS_FATAL("Parameter '~controller_manager' not specified. This is needed to identify the plugin to use for interacting with controllers. No paths can be executed.");
  }
  
  try
  {
    controller_manager_.reset(controller_manager_loader_->createUnmanagedInstance(controller));
  }
  catch(pluginlib::PluginlibException& ex)
  {
    ROS_FATAL_STREAM("Exception while loading controller manager '" << controller << "': " << ex.what());
  } */
  controller_manager_.reset(new test_moveit_controller_manager::TestMoveItControllerManager());
  
  // other configuration steps
  reloadControllerInformation();
  
  root_node_handle_.subscribe("trajectory_execution_event", 100, &TrajectoryExecutionManager::receiveEvent, this);
  
  if (manage_controllers_)
    ROS_INFO("Trajectory execution is managing controllers");
  else
    ROS_INFO("Trajectory execution is not managing controllers");
}

bool TrajectoryExecutionManager::isManagingControllers(void) const
{
  return manage_controllers_;
}

const moveit_controller_manager::MoveItControllerManagerPtr& TrajectoryExecutionManager::getControllerManager(void)
{
  return controller_manager_;
}

void TrajectoryExecutionManager::processEvent(const std::string &event)
{
  if (event == "stop")
    stopExecution(true);
  else
    ROS_WARN_STREAM("Unknown event type: '" << event << "'");
}

void TrajectoryExecutionManager::receiveEvent(const std_msgs::StringConstPtr &event)
{
  ROS_INFO_STREAM("Received event '" << event->data << "'");
  processEvent(event->data);
}

bool TrajectoryExecutionManager::push(const moveit_msgs::RobotTrajectory &trajectory, const std::string &controller)
{
  if (controller.empty())
    return push(trajectory, std::vector<std::string>());
  else    
    return push(trajectory, std::vector<std::string>(1, controller));    
}  

bool TrajectoryExecutionManager::push(const trajectory_msgs::JointTrajectory &trajectory, const std::string &controller)
{
  if (controller.empty())
    return push(trajectory, std::vector<std::string>());
  else    
    return push(trajectory, std::vector<std::string>(1, controller));
}

bool TrajectoryExecutionManager::push(const trajectory_msgs::JointTrajectory &trajectory, const std::vector<std::string> &controllers)
{
  moveit_msgs::RobotTrajectory traj;
  traj.joint_trajectory = trajectory;
  return push(traj, controllers);
}

bool TrajectoryExecutionManager::push(const moveit_msgs::RobotTrajectory &trajectory, const std::vector<std::string> &controllers)
{
  TrajectoryExecutionContext context;
  if (configure(context, trajectory, controllers))
  {
    trajectories_.push_back(context);
    return true;
  }
  return false;
}

void TrajectoryExecutionManager::reloadControllerInformation(void)
{
  known_controllers_.clear();
  if (controller_manager_)
  {
    std::vector<std::string> names;
    controller_manager_->getControllersList(names);
    for (std::size_t i = 0 ; i < names.size() ; ++i)
    {
      std::vector<std::string> joints;
      controller_manager_->getControllerJoints(names[i], joints);
      ControllerInformation ci;
      ci.name_ = names[i];
      ci.joints_.insert(joints.begin(), joints.end());
      known_controllers_[ci.name_] = ci;
    }

    for (std::map<std::string, ControllerInformation>::iterator it = known_controllers_.begin() ; it != known_controllers_.end() ; ++it)
      for (std::map<std::string, ControllerInformation>::iterator jt = known_controllers_.begin() ; jt != known_controllers_.end() ; ++jt)
        if (it != jt)
        {
          std::vector<std::string> intersect;
          std::set_intersection(it->second.joints_.begin(), it->second.joints_.end(),
                                jt->second.joints_.begin(), jt->second.joints_.end(),
                                std::back_inserter(intersect)); 
          if (!intersect.empty())
          {
            it->second.overlapping_controllers_.insert(jt->first);
            jt->second.overlapping_controllers_.insert(it->first);
          }
        }
  }
}

void TrajectoryExecutionManager::updateControllerState(const std::string &controller, const ros::Duration &age)
{
  std::map<std::string, ControllerInformation>::iterator it = known_controllers_.find(controller);
  if (it != known_controllers_.end())
    updateControllerState(it->second, age);
  else
    ROS_ERROR("Controller '%s' is not known.", controller.c_str());
}

void TrajectoryExecutionManager::updateControllerState(ControllerInformation &ci, const ros::Duration &age)
{
  if (ros::Time::now() - ci.last_update_ >= age)
    if (controller_manager_)
    {
      ci.state_ = controller_manager_->getControllerState(ci.name_);
      ci.last_update_ = ros::Time::now();
    }
}

void TrajectoryExecutionManager::updateControllersState(const ros::Duration &age)
{
  for (std::map<std::string, ControllerInformation>::iterator it = known_controllers_.begin() ; it != known_controllers_.end() ; ++it)
    updateControllerState(it->second, age);
}

bool TrajectoryExecutionManager::checkControllerCombination(std::vector<std::string> &selected, const std::set<std::string> &actuated_joints)
{
  std::set<std::string> combined_joints;
  for (std::size_t i = 0 ; i < selected.size() ; ++i)
  {
    const ControllerInformation &ci = known_controllers_[selected[i]];
    combined_joints.insert(ci.joints_.begin(), ci.joints_.end());
  }
  return std::includes(combined_joints.begin(), combined_joints.end(),
                       actuated_joints.begin(), actuated_joints.end());
}

void TrajectoryExecutionManager::generateControllerCombination(std::size_t start_index, std::size_t controller_count,
                                                               const std::vector<std::string> &available_controllers, 
                                                               std::vector<std::string> &selected_controllers,
                                                               std::vector< std::vector<std::string> > &selected_options,
                                                               const std::set<std::string> &actuated_joints)
{
  if (selected_controllers.size() == controller_count)
  {
    if (checkControllerCombination(selected_controllers, actuated_joints))
      selected_options.push_back(selected_controllers);
    return;
  }
  
  for (std::size_t i = start_index ; i < available_controllers.size() ; ++i)
  {
    bool overlap = false;
    const ControllerInformation &ci = known_controllers_[available_controllers[i]];
    for (std::size_t j = 0 ; j < selected_controllers.size() && !overlap ; ++j)
    {
      if (ci.overlapping_controllers_.find(selected_controllers[j]) != ci.overlapping_controllers_.end())
        overlap = true;
    }
    if (overlap)
      continue;
    selected_controllers.push_back(available_controllers[i]);
    generateControllerCombination(i + 1, controller_count, available_controllers, selected_controllers, selected_options, actuated_joints);
    selected_controllers.pop_back();
  }
}

bool TrajectoryExecutionManager::findControllers(const std::set<std::string> &actuated_joints, std::size_t controller_count, const std::vector<std::string> &available_controllers, std::vector<std::string> &selected_controllers)
{
  // generate all combinations of controller_count controllers that operate on disjoint sets of joints 
  std::vector<std::string> work_area;
  std::vector< std::vector<std::string> > selected_options;
  generateControllerCombination(0, controller_count, available_controllers, work_area, selected_options, actuated_joints);

  // if none was found, this is a problem
  if (selected_options.empty())
    return false;

  // if only one was found, return it
  if (selected_options.size() == 1)
  {
    selected_controllers.swap(selected_options[0]);
    return true;
  }
  
  // if more options were found, evaluate them all and return the best one
  // preference is given to controllers marked as default
  // and then to ones that are already active 
  
  // depending on whether we are allowed to load & unload controllers, 
  // we have different preference on deciding between options
  if (!manage_controllers_)
  {
    // if we can't load different options at will, just choose one that is already loaded
    for (std::size_t i = 0 ; i < selected_options.size() ; ++i)
      if (areControllersActive(selected_options[i]))
      {
        selected_controllers.swap(selected_options[i]);
        return true;
      }
  }
  
  // find the number of default controllers for each of the found options
  std::vector<std::size_t> nrdefault(selected_options.size(), 0);
  std::size_t max_nrdefault = 0;
  std::size_t max_nrdefault_count = 0;
  std::size_t max_nrdefault_index = 0;
  for (std::size_t i = 0 ; i < selected_options.size() ; ++i)
  {
    for (std::size_t k = 0 ; k < selected_options[i].size() ; ++k)
      if (known_controllers_[selected_options[i][k]].default_)
        nrdefault[i]++;
    if (nrdefault[i] > max_nrdefault)
    {
      max_nrdefault = nrdefault[i];
      max_nrdefault_count = 1;
      max_nrdefault_index = i;
    }
    else
      if (nrdefault[i] == max_nrdefault)
        max_nrdefault_count++;
  }
  
  // if there are multiple options with the same number of default controllers, choose the first option that has all 
  // controllers already active
  if (max_nrdefault_count > 1)
    for (std::size_t i = max_nrdefault_index ; i < selected_options.size() ; ++i)
      if (nrdefault[i] == max_nrdefault)
      {
        if (areControllersActive(selected_options[i]))
        {
          selected_controllers.swap(selected_options[i]);
          return true;
        }
      }
  // otherwise, just use the first valid option
  selected_controllers.swap(selected_options[max_nrdefault_index]);
  return true;
}


bool TrajectoryExecutionManager::areControllersActive(const std::vector<std::string> &controllers)
{
  static const ros::Duration default_age(1.0);
  for (std::size_t i = 0 ; i < controllers.size() ; ++i)
  {
    updateControllerState(controllers[i], default_age);
    std::map<std::string, ControllerInformation>::iterator it = known_controllers_.find(controllers[i]);
    if (it == known_controllers_.end() || !it->second.state_.active_)
      return false;
  }
  return true;
}

bool TrajectoryExecutionManager::selectControllers(const std::set<std::string> &actuated_joints, const std::vector<std::string> &available_controllers, std::vector<std::string> &selected_controllers)
{
  for (std::size_t i = 1 ; i <= available_controllers.size() ; ++i)
    if (findControllers(actuated_joints, i, available_controllers, selected_controllers))
    {
      // if we are not managing controllers, prefer to use active controllers even if there are more of them
      if (!manage_controllers_ && !areControllersActive(selected_controllers))
      {
        std::vector<std::string> other_option;
        for (std::size_t j = i + 1 ; j <= available_controllers.size() ; ++j)
          if (findControllers(actuated_joints, j, available_controllers, other_option))
          {
            if (areControllersActive(other_option))
            {
              selected_controllers = other_option;
              break;
            }
          }
      }
      return true;
    }
  return false;
}

bool TrajectoryExecutionManager::distributeTrajectory(const moveit_msgs::RobotTrajectory &trajectory, const std::vector<std::string> &controllers, std::vector<moveit_msgs::RobotTrajectory> &parts)
{
  parts.clear();
  parts.resize(controllers.size());
  
  std::set<std::string> actuated_joints_mdof;
  actuated_joints_mdof.insert(trajectory.multi_dof_joint_trajectory.joint_names.begin(),
                              trajectory.multi_dof_joint_trajectory.joint_names.end());
  std::set<std::string> actuated_joints_single;
  actuated_joints_single.insert(trajectory.joint_trajectory.joint_names.begin(),
                                trajectory.joint_trajectory.joint_names.end());
  
  for (std::size_t i = 0 ; i < controllers.size() ; ++i)
  {
    std::map<std::string, ControllerInformation>::iterator it = known_controllers_.find(controllers[i]);
    if (it == known_controllers_.end())
    {
      ROS_ERROR_STREAM("Controller " << controllers[i] << " not found.");
      return false;
    }
    std::vector<std::string> intersect_mdof;
    std::set_intersection(it->second.joints_.begin(), it->second.joints_.end(),
                          actuated_joints_mdof.begin(), actuated_joints_mdof.end(),
                          std::back_inserter(intersect_mdof));
    std::vector<std::string> intersect_single;
    std::set_intersection(it->second.joints_.begin(), it->second.joints_.end(),
                          actuated_joints_single.begin(), actuated_joints_single.end(),
                          std::back_inserter(intersect_single));
    if (intersect_mdof.empty() && intersect_single.empty())
      ROS_WARN_STREAM("No joints to be distributed for controller " << controllers[i]);
    {
      if (!intersect_mdof.empty())
      {
        std::vector<std::string> &jnames = parts[i].multi_dof_joint_trajectory.joint_names;
        jnames.insert(jnames.end(), intersect_mdof.begin(), intersect_mdof.end());
        parts[i].multi_dof_joint_trajectory.frame_ids.resize(jnames.size());
        parts[i].multi_dof_joint_trajectory.child_frame_ids.resize(jnames.size());
        std::map<std::string, std::size_t> index;
        for (std::size_t j = 0 ; j < trajectory.multi_dof_joint_trajectory.joint_names.size() ; ++j)
          index[trajectory.multi_dof_joint_trajectory.joint_names[j]] = j;
        std::vector<std::size_t> bijection(jnames.size());
        for (std::size_t j = 0 ; j < jnames.size() ; ++j)
        {
          bijection[j] = index[jnames[j]];
          parts[i].multi_dof_joint_trajectory.frame_ids[j] = trajectory.multi_dof_joint_trajectory.frame_ids[bijection[j]];
          parts[i].multi_dof_joint_trajectory.child_frame_ids[j] = trajectory.multi_dof_joint_trajectory.child_frame_ids[bijection[j]];
        }
        parts[i].multi_dof_joint_trajectory.points.resize(trajectory.multi_dof_joint_trajectory.points.size());
        for (std::size_t j = 0 ; j < trajectory.multi_dof_joint_trajectory.points.size() ; ++j)
        {
          parts[i].multi_dof_joint_trajectory.points[j].time_from_start = trajectory.multi_dof_joint_trajectory.points[j].time_from_start;
          parts[i].multi_dof_joint_trajectory.points[j].poses.resize(bijection.size());
          for (std::size_t k = 0 ; k < bijection.size() ; ++k)
            parts[i].multi_dof_joint_trajectory.points[j].poses[k] = trajectory.multi_dof_joint_trajectory.points[j].poses[bijection[k]];
        }        
      }
      if (!intersect_single.empty())
      {
        std::vector<std::string> &jnames = parts[i].joint_trajectory.joint_names;
        jnames.insert(jnames.end(), intersect_single.begin(), intersect_single.end());
        parts[i].joint_trajectory.header = trajectory.joint_trajectory.header;
        std::map<std::string, std::size_t> index;
        for (std::size_t j = 0 ; j < trajectory.joint_trajectory.joint_names.size() ; ++j)
          index[trajectory.joint_trajectory.joint_names[j]] = j;
        std::vector<std::size_t> bijection(jnames.size());
        for (std::size_t j = 0 ; j < jnames.size() ; ++j)
          bijection[j] = index[jnames[j]];
        parts[i].joint_trajectory.points.resize(trajectory.joint_trajectory.points.size());
        for (std::size_t j = 0 ; j < trajectory.joint_trajectory.points.size() ; ++j)
        {
          parts[i].joint_trajectory.points[j].time_from_start = trajectory.joint_trajectory.points[j].time_from_start;
          if (!trajectory.joint_trajectory.points[j].positions.empty())
          {
            parts[i].joint_trajectory.points[j].positions.resize(bijection.size());
            for (std::size_t k = 0 ; k < bijection.size() ; ++k)
              parts[i].joint_trajectory.points[j].positions[k] = trajectory.joint_trajectory.points[j].positions[bijection[k]];
          }
          if (!trajectory.joint_trajectory.points[j].velocities.empty())
          {
            parts[i].joint_trajectory.points[j].velocities.resize(bijection.size());
            for (std::size_t k = 0 ; k < bijection.size() ; ++k)
              parts[i].joint_trajectory.points[j].velocities[k] = trajectory.joint_trajectory.points[j].velocities[bijection[k]];
          }
          if (!trajectory.joint_trajectory.points[j].accelerations.empty())
          {
            parts[i].joint_trajectory.points[j].accelerations.resize(bijection.size());
            for (std::size_t k = 0 ; k < bijection.size() ; ++k)
              parts[i].joint_trajectory.points[j].accelerations[k] = trajectory.joint_trajectory.points[j].accelerations[bijection[k]];
          }
        } 
      }
    }
  }
  return true;
}

bool TrajectoryExecutionManager::configure(TrajectoryExecutionContext &context, const moveit_msgs::RobotTrajectory &trajectory, const std::vector<std::string> &controllers)
{
  std::set<std::string> actuated_joints;
  actuated_joints.insert(trajectory.multi_dof_joint_trajectory.joint_names.begin(),
                         trajectory.multi_dof_joint_trajectory.joint_names.end());
  actuated_joints.insert(trajectory.joint_trajectory.joint_names.begin(),
                         trajectory.joint_trajectory.joint_names.end());
  
  if (controllers.empty())
  {
    bool retry = true;
    bool reloaded = false;
    while (retry)
    {
      retry = false;
      std::vector<std::string> all_controller_names;    
      for (std::map<std::string, ControllerInformation>::const_iterator it = known_controllers_.begin() ; it != known_controllers_.end() ; ++it)
        all_controller_names.push_back(it->first);
      if (selectControllers(actuated_joints, all_controller_names, context.controllers_))
      {
        if (distributeTrajectory(trajectory, context.controllers_, context.trajectory_parts_))
          return true;
      }
      else
      {
        // maybe we failed because we did not have a complete list of controllers
        if (!reloaded)
        {
          reloadControllerInformation();
          reloaded = true;
          retry = true;
        }
      }
    }
  }
  else
  {
    // check if the specified controllers are valid names;
    // if they appear not to be, try to reload the controller information, just in case they are new in the system
    bool reloaded = false;
    for (std::size_t i = 0 ; i < controllers.size() ; ++i)
      if (known_controllers_.find(controllers[i]) == known_controllers_.end())
      {
        reloadControllerInformation();
        reloaded = true;
        break;
      }
    if (reloaded)
      for (std::size_t i = 0 ; i < controllers.size() ; ++i)
        if (known_controllers_.find(controllers[i]) == known_controllers_.end())
        {
          ROS_ERROR("Controller '%s' is not known", controllers[i].c_str());
          return false;
        }
    if (selectControllers(actuated_joints, controllers, context.controllers_))
    {
      if (distributeTrajectory(trajectory, context.controllers_, context.trajectory_parts_))
        return true;
    }
  }
  return false;
}

void TrajectoryExecutionManager::executeAndWait(bool auto_clear)
{
  execute(ExecutionCompleteCallback(), auto_clear);
  waitForExecution();
}

void TrajectoryExecutionManager::stopExecution(bool auto_clear)
{
  if (!execution_complete_)
  {
    execution_complete_ = true;
    execution_thread_->join();
    execution_thread_.reset();
    if (auto_clear)
      clear();
  }
  else
    if (execution_thread_) // just in case we have some thread waiting to be joined from some point in the past, we join it now
    {  
      execution_thread_->join();
      execution_thread_.reset();
    }
}

void TrajectoryExecutionManager::execute(const ExecutionCompleteCallback &callback, bool auto_clear)
{
  stopExecution(false);
  execution_complete_ = false;
  execution_thread_.reset(new boost::thread(&TrajectoryExecutionManager::executeThread, this, callback, auto_clear));
}

void TrajectoryExecutionManager::waitForExecution(void)
{
  boost::unique_lock<boost::mutex> ulock(execution_state_mutex_);
  while (!execution_complete_)
    execution_complete_condition_.wait(ulock);
  // this will join the thread
  stopExecution(false);
}

void TrajectoryExecutionManager::clear(void)
{
  trajectories_.clear();
}

void TrajectoryExecutionManager::executeThread(const ExecutionCompleteCallback &callback, bool auto_clear)
{
  bool ok = true;
  for (std::size_t i = 0 ; i < trajectories_.size() ; ++i)
    if (!executePart(trajectories_[i]))
    {
      ok = false;
      break;
    }
  
  if (callback)
    callback(ok);
  if (auto_clear)
    clear();
  
  execution_state_mutex_.lock();
  execution_complete_ = true;
  execution_state_mutex_.unlock();
  execution_complete_condition_.notify_all();
}

bool TrajectoryExecutionManager::executePart(TrajectoryExecutionContext &context)
{
  if (ensureActiveControllers(context.controllers_))
  { 
    std::vector<moveit_controller_manager::MoveItControllerHandlePtr> handles(context.controllers_.size());
    for (std::size_t i = 0 ; i < context.controllers_.size() ; ++i)
      handles[i] = controller_manager_->getControllerHandle(context.controllers_[i]);
    for (std::size_t i = 0 ; i < context.trajectory_parts_.size() ; ++i)
      if (!handles[i]->sendTrajectory(context.trajectory_parts_[i]))
      {
        for (std::size_t j = 0 ; j < i ; ++j)
          handles[j]->cancelExecution();
        return false;
      }
    bool result = true;
    for (std::size_t i = 0 ; i < context.trajectory_parts_.size() ; ++i)
    {
      handles[i]->waitForExecution();
      if (handles[i]->getLastExecutionStatus() != moveit_controller_manager::ExecutionStatus::SUCCEEDED)
        result = false;
    }
    return result;
  }
  else
    return false;
}

bool TrajectoryExecutionManager::ensureActiveControllersForGroup(const std::string &group)
{
  const planning_models::KinematicModel::JointModelGroup *joint_model_group = kinematic_model_->getJointModelGroup(group);
  if (joint_model_group)
    return ensureActiveControllersForJoints(joint_model_group->getJointModelNames());
  else
    return false;
}

bool TrajectoryExecutionManager::ensureActiveControllersForJoints(const std::vector<std::string> &joints)
{ 
  std::vector<std::string> all_controller_names;    
  for (std::map<std::string, ControllerInformation>::const_iterator it = known_controllers_.begin() ; it != known_controllers_.end() ; ++it)
    all_controller_names.push_back(it->first);
  std::vector<std::string> selected_controllers;
  std::set<std::string> jset;
  jset.insert(joints.begin(), joints.end());
  if (selectControllers(jset, all_controller_names, selected_controllers))
    return ensureActiveControllers(selected_controllers);
  else
    return false;
}

bool TrajectoryExecutionManager::ensureActiveController(const std::string &controller)
{
  return ensureActiveControllers(std::vector<std::string>(1, controller));
}

bool TrajectoryExecutionManager::ensureActiveControllers(const std::vector<std::string> &controllers)
{
  updateControllersState(ros::Duration(1.0));
  
  if (manage_controllers_)
  {
    std::vector<std::string> controllers_to_activate;
    std::vector<std::string> controllers_to_deactivate;
    std::set<std::string> joints_to_be_activated;
    std::set<std::string> joints_to_be_deactivated;
    for (std::size_t i = 0 ; i < controllers.size() ; ++i)
    {
      std::map<std::string, ControllerInformation>::const_iterator it = known_controllers_.find(controllers[i]);
      if (it == known_controllers_.end())
      {
        ROS_ERROR_STREAM("Controller " << controllers[i] << " is not known");
        return false;
      }
      if (!it->second.state_.active_)
      {      ROS_INFO_STREAM("Need to activate " << controllers[i]);

        controllers_to_activate.push_back(controllers[i]);
        joints_to_be_activated.insert(it->second.joints_.begin(), it->second.joints_.end());
        for (std::set<std::string>::iterator kt = it->second.overlapping_controllers_.begin() ; 
             kt != it->second.overlapping_controllers_.end() ; ++kt)
        {
          const ControllerInformation &ci = known_controllers_[*kt];
          if (ci.state_.active_)
          {
            controllers_to_deactivate.push_back(*kt);
            joints_to_be_deactivated.insert(ci.joints_.begin(), ci.joints_.end());
          }
        }
      }
      else
        ROS_INFO_STREAM("Controller " << controllers[i] << " is already active");
    }
    std::set<std::string> diff;
    std::set_difference(joints_to_be_deactivated.begin(), joints_to_be_deactivated.end(),
                        joints_to_be_activated.begin(), joints_to_be_activated.end(),
                        std::inserter(diff, diff.end()));
    if (!diff.empty())
    {
      // find the set of controllers that do not overlap with the ones we want to activate so far
      std::vector<std::string> possible_additional_controllers;
      for (std::map<std::string, ControllerInformation>::const_iterator it = known_controllers_.begin() ; it != known_controllers_.end() ; ++it)
      {
        bool ok = true;
        for (std::size_t k = 0 ; k < controllers_to_activate.size() ; ++k)
          if (it->second.overlapping_controllers_.find(controllers_to_activate[k]) != it->second.overlapping_controllers_.end())
          {
            ok = false;
            break;
          }
        if (ok)
          possible_additional_controllers.push_back(it->first);
      }
      
      // out of the allowable controllers, try to find a subset of controllers that covers the joints to be actuated
      std::vector<std::string> additional_controllers;
      if (selectControllers(diff, possible_additional_controllers, additional_controllers))
        controllers_to_activate.insert(controllers_to_activate.end(), additional_controllers.begin(), additional_controllers.end());
      else
        return false;
    }
    if (!controllers_to_activate.empty() || !controllers_to_deactivate.empty())
    {
      if (controller_manager_)
      {
        for (std::size_t a = 0 ; a < controllers_to_activate.size() ; ++a)
        {
          ControllerInformation &ci = known_controllers_[controllers_to_activate[a]];
          if (!ci.state_.loaded_)
            if (!controller_manager_->loadController(controllers_to_activate[a]))
              return false;
        }
        return controller_manager_->switchControllers(controllers_to_activate, controllers_to_deactivate);
      }
      else
        return false;
    }
    else
      return true;
  }
  else
  {   
    std::set<std::string> originally_active; 
    for (std::map<std::string, ControllerInformation>::const_iterator it = known_controllers_.begin() ; it != known_controllers_.end() ; ++it)
      if (it->second.state_.active_)
        originally_active.insert(it->first);
    return std::includes(originally_active.begin(), originally_active.end(), controllers.begin(), controllers.end());
  }
}


}