/*!
 * @defgroup quantile Quantile * @ingroup desc-stats *  * @about * \par
 * This function computes the specified quantile value. It reads the name of the table, the specific column, and * computes the quantile value based on the fraction specified as the third argument.  *  * \par
 * For a different implementation of quantile check out the cmsketch_centile() 
 * aggregate in \link countmin CountMin (Cormode-Muthukrishnan) \endlink module.  *  * @prereq
 * \par
 * None *  * @usage
 * \par
 * Function: <tt>quantile( '<em>table_name</em>', '<em>col_name</em>',
 *  <em>quantile</em>)</tt>
 * \par
 * Parameters:
 * - <em>table_name</em> (TEXT) - name of the table from which quantile is to be taken * - <em>col_name</em> (TEXT) - name of the column that is to be used for quantile calculation * - <em>quantile</em> (FLOAT) - desired quantile value \in (0,1) *  * @examp
 * \par
 * 1) Prepare some input:
 * \code * CREATE TABLE tab1 AS SELECT generate_series( 1,1000) as col1; * \endcode
 * \par
 * 2) Run the quantile() function:
 * \code * SELECT madlib.quantile( 'tab1', 'col1', .3); * \endcode * 
 * \see
 * \link countmin CountMin (Cormode-Muthukrishnan) \endlink
 */