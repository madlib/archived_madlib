# coding=utf-8

"""
@file logRegress.py_in

@brief Logistic Regression: Driver functions

@namespace logRegress

Logistic Regression: Driver functions
"""

import plpy

def __runIterativeAlg(stateType, initialState, source, updateExpr,
    terminateExpr, cyclesPerIteration, maxNumIterations,
    returnExpr = "st.state"):
    """
    Driver for an iterative algorithm
    
    A general driver function for most iterative algorithms: The state between
    iterations is kept in a variable of type <tt>stateType</tt>, which is
    initialized with <tt><em>initialState</em></tt>. During each iteration, the
    SQL statement <tt>updateSQL</tt> is executed in the database. Afterwards,
    the SQL query <tt>updateSQL</tt> decides whether the algorithm terminates.
    At the end of the algorithm, the last state (or an arbitrary transformation
    as given by <tt>returnExpr</tt>) will be returned.
    
    @param stateType SQL type of the state between iterations
    @param initialState The initial value of the SQL state variable
    @param source The source relation
    @param updateExpr SQL expression that returns the new state of type
        <tt>stateType</tt>. The expression may use the replacement fields
        <tt>"{state}"</tt>, <tt>"{iteration}"</tt>, and
        <tt>"{sourceAlias}"</tt>. Source alias is an alias for the source
        relation <tt><em>source</em></tt>.
    @param terminateExpr SQL expression that returns whether the algorithm should
        terminate. The expression may use the replacement fields
        <tt>"{oldState}"</tt>, <tt>"{newState}"</tt>, and
        <tt>"{iteration}"</tt>. It must return a BOOLEAN value.
    @param cyclesPerIteration Number of aggregate function calls per iteration.
    @param maxNumIterations Maximum number of iterations. Algorithm will then
        terminate even when <tt>terminateExpr</tt> does not evaluate to \c true
    @param returnExpr An optional transformation of the final state. If omitted,
        the final state will be returned. <tt>returnExpr</tt> can be, e.g., a
        function call or access to a tuple element. Use "{state}" to refer
        to the state variable. E.g., if <tt>stateType</tt> is a tuple containing
        a field <tt>coefficients</tt>, and this should be returned as the result
        of the algorithm, pass <tt>"{state}.coefficients"</tt> here. The
        replacement field <tt>"{iteration}"</tt> can also be used.
    """

    state = "(st.state)"
    sourceAlias = "src"
    oldState = "(older.state)"
    newState = "(newer.state)"
    
    updateExpr = updateExpr.format(**locals())
    terminateExpr = terminateExpr.format(**locals())
    returnExpr = returnExpr.format(**locals())

    updateSQL = """
        INSERT INTO _madlib_iterative_alg
        SELECT
            {iteration},
            {updateExpr}
        FROM
            _madlib_iterative_alg AS st,
            {source} AS src
        WHERE
            st.iteration = {iteration} - 1
        """    
    terminateSQL = """
        SELECT
            {terminateExpr} AS should_terminate
        FROM    
        (
            SELECT state
            FROM _madlib_iterative_alg
            WHERE iteration = {iteration} - {cyclesPerIteration}
        ) AS older,
        (
            SELECT state
            FROM _madlib_iterative_alg
            WHERE iteration = {iteration}
        ) AS newer
        """
    returnSQL = """
        SELECT
            {returnExpr} AS return_value
        FROM
            _madlib_iterative_alg AS st
        WHERE
            st.iteration = {iteration}
        """

    oldMsgLevel = plpy.execute("SHOW client_min_messages")[0]['client_min_messages']
    plpy.execute("""
        SET client_min_messages = error;
        DROP TABLE IF EXISTS _madlib_iterative_alg;
        CREATE TEMPORARY TABLE _madlib_iterative_alg (
            iteration INTEGER PRIMARY KEY,
            state {stateType}
        );
        SET client_min_messages = {oldMsgLevel};
        """.format(**locals()))
    
    iteration = 0
    plpy.execute("""
        INSERT INTO _madlib_iterative_alg VALUES ({iteration}, {initialState})
        """.format(**locals()))
    while True:
        iteration = iteration + 1
        plpy.execute(updateSQL.format(**locals()))
        if iteration > cyclesPerIteration and (
            iteration >= cyclesPerIteration * maxNumIterations or
            plpy.execute(terminateSQL.format(**locals()))[0]['should_terminate']
                == True):
            break
    
    # FIXME: Returning the result set from Python code means that values
    # pass through Python (and there is a potential loss of precision by
    # conversion)
    
    returnValue = plpy.execute(returnSQL.format(**locals()))[0]['return_value']
    plpy.execute("DROP TABLE _madlib_iterative_alg")
    return returnValue


def __cg_logregr_coef(**kwargs):
    """
    Logistic regression algorithm with the conjugate-gradient method
    
    The parameters are the same as for compute_logregr_coef(), except that
    <tt>optimizer</tt> should not be set. This function sets up all SQL
    expression as needed for the conjugate-gradient method and then calls
    __runIterativeAlg().
    """
    
    stateType = "FLOAT8[]"
    initialState = "NULL"
    source = kwargs['source']
    
    # "{state}", "{sourceAlias}", "{oldState}", and "{newState}" will not be
    # substituted here but will be passed on to __runIterativeAlg and
    # substituted there
    updateExpr = """
        logreg_cg_step(
            {{sourceAlias}}.{depColumn},
            {{sourceAlias}}.{indepColumn},
            {{state}}
        )
        """.format(**kwargs)
    if kwargs['precision'] == 0.:
        terminateExpr = "FALSE"
    else:
        terminateExpr = """
            _logreg_cg_step_distance({{newState}}, {{oldState}}) < {precision}
            """.format(**kwargs)
    
    # One iteration consists of two calls of the aggregate function
    # See float8_cg_update_final().
    cyclesPerIteration = 2
    maxNumIterations = kwargs['numIterations']
    returnExpr = "_logreg_cg_coef({state})"
    return __runIterativeAlg(stateType, initialState, source, updateExpr,
        terminateExpr, cyclesPerIteration, maxNumIterations, returnExpr)


def __irls__logregr_coef(**kwargs):
    """
    Logistic regression algorithm with the iteratively-reweighted-least-squares method
    
    The parameters are the same as for compute_logregr_coef(), except that
    <tt>optimizer</tt> should not be set. This function sets up all SQL
    expression as needed for the iteratively-reweighted-least-squares method and
    then calls __runIterativeAlg().
    """
    
    stateType = "FLOAT8[]"
    initialState = "NULL"
    source = kwargs['source']
    updateExpr = """
        logreg_irls_step(
            {{sourceAlias}}.{depColumn},
            {{sourceAlias}}.{indepColumn},
            {{state}}
        )
        """.format(**kwargs)
    if kwargs['precision'] == 0.:
        terminateExpr = "FALSE"
    else:
        terminateExpr = """
            _logreg_irls_step_distance({{newState}}, {{oldState}}) < {precision}
            """.format(**kwargs)

    cyclesPerIteration = 1
    maxNumIterations = kwargs['numIterations']
    returnExpr = "_logreg_irls_coef({state})"
    return __runIterativeAlg(stateType, initialState, source, updateExpr,
        terminateExpr, cyclesPerIteration, maxNumIterations, returnExpr)
    

def compute_logregr_coef(**kwargs):
    """
    Compute logistic regression coefficients
    
    This method serves as an interface to different optimization algorithms.
    By default, iteratively reweighted least squares is used, but for data with
    a lot of columns the conjugate-gradient method might perform better.
    
    @param source Name of relation containing the training data
    @param depColumn Name of dependent column in training data (of type BOOLEAN)
    @param indepColumn Name of independent column in training data (of type
           DOUBLE PRECISION[])
    
    Optionally also provide the following:
    @param optimizer Name of the optimizer. 'newton' or 'irls': Iteratively
        reweighted least squares, 'cg': conjugate gradient (default = 'irls')
    @param numIterations Maximum number of iterations (default = 20)
    @param precision Terminate if two consecutive iterations have a difference 
           in the log-likelihood of less than <tt>precision</tt>. In other
           words, we terminate if the objective function value has converged.
           If this parameter is 0.0, then the algorithm will not check for
           convergence and only terminate after <tt>numIterations</tt>
           iterations.
    
    @return array with coefficients in case of convergence, otherwise None
    
    """
    if not 'optimizer' in kwargs:
        kwargs.update(optimizer = 'irls')
    if not 'numIterations' in kwargs:
        kwargs.update(numIterations = 20)
    if not 'precision' in kwargs:
        kwargs.update(precision = 0.0001)
        
    if kwargs['optimizer'] == 'cg':
        return __cg_logregr_coef(**kwargs)
    elif kwargs['optimizer'] in ['irls', 'newton']:
        return __irls__logregr_coef(**kwargs)
    else:
        plpy.error("Unknown optimizer requested. Must be 'newton'/'irls' or 'cg'")
    
    return None
