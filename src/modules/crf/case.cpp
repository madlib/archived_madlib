#include "lbfgs.cpp"
class MyObjective : public drwnOptimizer
{
	public: 
        MyObjective() : drwnOptimizer(1) { }
	~MyObjective() { }

	double objective(const double *x) const {
		return (x[0] - 2.0) * (x[0] - 4.0);
	}

	void gradient(const double *x, double *df) const {
		df[0] = 2.0 * (x[0] - 3.0);
	}
};

int main()
{
	MyObjective objFunction;

	double f_star = objFunction.solve(1000);
	double x_star = objFunction[0];

	cout << "f(" << x_star << ") = " << f_star << endl;
	return 0;
}
