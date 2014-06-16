set -x
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# this test scripts assume JPMML installed in the same code path as madlib:
# /path/to/code/madlib and /path/to/code/jpmml
pathtocode="$( cd "$( dirname "${BASH_SOURCE[0]}" )"/../../../../../../.. && pwd )"
cd ${dir}
mkdir -p /tmp/pmml
psql -f copy_in.sql > /dev/null
cd ${pathtocode}/jpmml-evaluator/pmml-evaluator-example
psql -A -t -c "SELECT madlib.pmml('no_grouping')" > no_grouping.pmml
psql -A -t -c "SELECT madlib.pmml('simple_grouping')" > simple_grouping.pmml
psql -A -t -c "SELECT madlib.pmml('complex_grouping')" > complex_grouping.pmml
psql -A -t -c "SELECT madlib.pmml('linregr_grouping', 'c.price ~ c.1 + c.bedroom + c.bath + c.size')" > linregr_grouping.pmml
rm -f out_no_grouping.csv out_simple_grouping.csv out_complex_grouping.csv out_linregr_grouping.csv
java \
    -cp target/example-1.1-SNAPSHOT.jar org.jpmml.evaluator.CsvEvaluationExample \
    --model no_grouping.pmml \
    --input /tmp/pmml/in_no_grouping.csv \
    --output /tmp/pmml/out_no_grouping.csv
java \
    -cp target/example-1.1-SNAPSHOT.jar org.jpmml.evaluator.CsvEvaluationExample \
    --model simple_grouping.pmml \
    --input /tmp/pmml/in_simple_grouping.csv \
    --output /tmp/pmml/out_simple_grouping.csv
java \
    -cp target/example-1.1-SNAPSHOT.jar org.jpmml.evaluator.CsvEvaluationExample \
    --model complex_grouping.pmml \
    --input /tmp/pmml/in_complex_grouping.csv \
    --output /tmp/pmml/out_complex_grouping.csv
java \
    -cp target/example-1.1-SNAPSHOT.jar org.jpmml.evaluator.CsvEvaluationExample \
    --model linregr_grouping.pmml \
    --input /tmp/pmml/in_linregr_grouping.csv \
    --output /tmp/pmml/out_linregr_grouping.csv
psql -f ${pathtocode}/madlib/src/ports/postgres/modules/pmml/test/compare_prediction.sql
