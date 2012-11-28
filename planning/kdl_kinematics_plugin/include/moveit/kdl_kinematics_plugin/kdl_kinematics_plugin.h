/*********************************************************************
*
* Software License Agreement (BSD License)
*
*  Copyright (c) 2012, Willow Garage, Inc.
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
*   * Neither the name of Willow Garage, Inc. nor the names of its
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
*
* Author: Sachin Chitta, David Lu!!, Ugo Cupcic
*********************************************************************/

#ifndef MOVEIT_ROS_PLANNING_KDL_KINEMATICS_PLUGIN_
#define MOVEIT_ROS_PLANNING_KDL_KINEMATICS_PLUGIN_

// ROS
#include <ros/ros.h>
#include <random_numbers/random_numbers.h>

// System
#include <boost/shared_ptr.hpp>

// ROS msgs
#include <geometry_msgs/PoseStamped.h>
#include <moveit_msgs/GetPositionFK.h>
#include <moveit_msgs/GetPositionIK.h>
#include <moveit_msgs/GetKinematicSolverInfo.h>
#include <moveit_msgs/MoveItErrorCodes.h>

// KDL
#include <kdl/jntarray.hpp>
#include <kdl/chainiksolvervel_pinv.hpp>
#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <kdl/chainfksolverpos_recursive.hpp>

// MoveIt!
#include <moveit/kinematics_base/kinematics_base.h>
#include <moveit/robot_model_loader/robot_model_loader.h>

namespace kdl_kinematics_plugin                        
{
/**
 * @class Specific implementation of kinematics using KDL. This version can be used with any robot.
 */
  class KDLKinematicsPlugin : public kinematics::KinematicsBase
  {
    public:

    /** @class
     *  @brief Plugin-able interface to a generic version of arm kinematics
     */
    KDLKinematicsPlugin();

    virtual bool getPositionIK(const geometry_msgs::Pose &ik_pose,
                               const std::vector<double> &ik_seed_state,
                               std::vector<double> &solution,
                               moveit_msgs::MoveItErrorCodes &error_code) const;      
    
    virtual bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                                  const std::vector<double> &ik_seed_state,
                                  double timeout,
                                  std::vector<double> &solution,
                                  moveit_msgs::MoveItErrorCodes &error_code) const;      

    virtual bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                                  const std::vector<double> &ik_seed_state,
                                  double timeout,
                                  unsigned int redundancy,         
                                  double consistency_limit,
                                  std::vector<double> &solution,
                                  moveit_msgs::MoveItErrorCodes &error_code) const;      

    virtual bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                                  const std::vector<double> &ik_seed_state,
                                  double timeout,
                                  std::vector<double> &solution,
                                  const IKCallbackFn &solution_callback,
                                  moveit_msgs::MoveItErrorCodes &error_code) const;
    
    virtual bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                                  const std::vector<double> &ik_seed_state,
                                  double timeout,
                                  unsigned int redundancy,
                                  double consistency_limit,
                                  std::vector<double> &solution,
                                  const IKCallbackFn &solution_callback,
                                  moveit_msgs::MoveItErrorCodes &error_code) const;      

    virtual bool getPositionFK(const std::vector<std::string> &link_names,
                               const std::vector<double> &joint_angles, 
                               std::vector<geometry_msgs::Pose> &poses) const;
    
    virtual bool initialize(const std::string &group_name,
                            const std::string &base_name,
                            const std::string &tip_name,
                            double search_discretization);
        
    /**
     * @brief  Return all the joint names in the order they are used internally
     */
    const std::vector<std::string>& getJointNames() const;
    
    /**
     * @brief  Return all the link names in the order they are represented internally
     */
    const std::vector<std::string>& getLinkNames() const;
    
  protected:

  /**
   * @brief Given a desired pose of the end-effector, search for the joint angles required to reach it.
   * This particular method is intended for "searching" for a solutions by stepping through the redundancy
   * (or other numerical routines).
   * @param ik_pose the desired pose of the link
   * @param ik_seed_state an initial guess solution for the inverse kinematics
   * @param timeout The amount of time (in seconds) available to the solver
   * @param solution the solution vector
   * @param solution_callback A callback solution for the IK solution
   * @param error_code an error code that encodes the reason for failure or success
   * @param max_search_iteration The maximum number of times the search will restart
   * @param check_consistency Set to true if consistency check needs to be performed
   * @param redundancy The index of the redundant joint
   * @param consistency_limit The returned solutuion will contain a value for the redundant joint in the range [seed_state(redundancy_limit)-consistency_limit,seed_state(redundancy_limit)+consistency_limit]
   * @return True if a valid solution was found, false otherwise
   */
    bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                          const std::vector<double> &ik_seed_state,
                          double timeout,
                          std::vector<double> &solution,
                          const IKCallbackFn &solution_callback,
                          moveit_msgs::MoveItErrorCodes &error_code,
                          unsigned int max_search_iterations,
                          bool check_consistency,
                          unsigned int redundancy=0,
                          double consistency_limit=-1.0) const;
    
  private:
    
    bool timedOut(const ros::WallTime &start_time, double duration) const;
    
    
    /** @brief Check whether the solution lies within the consistency limit of the seed state
     *  @param seed_state Seed state
     *  @param redundancy Index of the redundant joint within the chain
     *  @param consistency_limit The returned state for redundant joint should be in the range [seed_state(redundancy_limit)-consistency_limit,seed_state(redundancy_limit)+consistency_limit]
     *  @param solution solution configuration
     *  @return true if check succeeds
     */
    bool checkConsistency(const KDL::JntArray& seed_state,
                          const unsigned int& redundancy,
                          const double& consistency_limit,
                          const KDL::JntArray& solution) const;

    int getJointIndex(const std::string &name) const;

    int getKDLSegmentIndex(const std::string &name) const;

    void getRandomConfiguration(KDL::JntArray &jnt_array) const;

    /** @brief Get a random configuration within joint limits close to the seed state
     *  @param seed_state Seed state
     *  @param redundancy Index of the redundant joint within the chain
     *  @param consistency_limit The returned state will contain a value for the redundant joint in the range [seed_state(redundancy_limit)-consistency_limit,seed_state(redundancy_limit)+consistency_limit]
     *  @param jnt_array Returned random configuration
     */
    void getRandomConfiguration(const KDL::JntArray& seed_state,
                                const unsigned int& redundancy,
                                const double& consistency_limit,
                                KDL::JntArray &jnt_array) const;
    
    int max_search_iterations_; /** max number of search iterations for the kinematics solver */

    bool active_; /** Internal variable that indicates whether solvers are configured and ready */

    moveit_msgs::KinematicSolverInfo ik_chain_info_; /** Stores information for the inverse kinematics solver */

    moveit_msgs::KinematicSolverInfo fk_chain_info_; /** Store information for the forward kinematics solver */

    KDL::Chain kdl_chain_; 

    boost::shared_ptr<KDL::ChainIkSolverVel_pinv> ik_solver_vel_; /** KDL IK velocity solver */

    boost::shared_ptr<KDL::ChainFkSolverPos_recursive> fk_solver_;/** KDL FK solver */

    boost::shared_ptr<KDL::ChainIkSolverPos_NR_JL> ik_solver_pos_;/** KDL IK position solver */ 
    
    unsigned int dimension_; /** Dimension of the group */

    KDL::JntArray joint_min_, joint_max_; /** Joint limits */

    robot_model_loader::RobotModelLoader robot_model_loader_; /** Used to load the robot model */

    mutable KDL::JntArray jnt_seed_state_,jnt_pos_in_,jnt_pos_out_;/** Pre-allocated for the number of joints (hence mutable) */

    mutable random_numbers::RandomNumberGenerator random_number_generator_;
    
  };
}

#endif