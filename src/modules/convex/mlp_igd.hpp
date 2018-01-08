/* ----------------------------------------------------------------------- *//**
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
 *
 *
 * @file mlp_igd.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Multilayer perceptron (incremental gradient): Transition function
 */
DECLARE_UDF(convex, mlp_igd_transition)
DECLARE_UDF(convex, mlp_minibatch_transition)

/**
 * @brief Multilayer perceptron (incremental gradient): State merge function
 */
DECLARE_UDF(convex, mlp_igd_merge)
DECLARE_UDF(convex, mlp_minibatch_merge)

/**
 * @brief Multilayer perceptron (incremental gradient): Final function
 */
DECLARE_UDF(convex, mlp_igd_final)
DECLARE_UDF(convex, mlp_minibatch_final)

/**
 * @brief Multilayer perceptron (incremental gradient): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(convex, internal_mlp_igd_distance)
DECLARE_UDF(convex, internal_mlp_minibatch_distance)

/**
 * @brief Multilayer perceptron (incremental gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_mlp_igd_result)
DECLARE_UDF(convex, internal_mlp_minibatch_result)

/**
 * @brief Multilayer perceptron (incremental gradient): Predict
 *      function for regression and classification probability
 */

DECLARE_UDF(convex, internal_predict_mlp)
