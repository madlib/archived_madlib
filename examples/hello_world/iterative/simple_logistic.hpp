/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Logistic regression (conjugate-gradient step): Transition function
 */
DECLARE_UDF(hello_world, logregr_simple_step_transition)

/**
 * @brief Logistic regression (conjugate-gradient step): State merge function
 */
DECLARE_UDF(hello_world, logregr_simple_step_merge_states)

/**
 * @brief Logistic regression (conjugate-gradient step): Final function
 */
DECLARE_UDF(hello_world, logregr_simple_step_final)

/**
 * @brief Logistic regression (conjugate-gradient): Difference in log-likelihood
 *     between two transition states
 */
DECLARE_UDF(hello_world, internal_logregr_simple_step_distance)

/**
 * @brief Logistic regression (conjugate-gradient): Convert transition state to 
 *     result tuple
 */
DECLARE_UDF(hello_world, internal_logregr_simple_result)
