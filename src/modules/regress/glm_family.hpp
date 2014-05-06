/* ----------------------------------------------------------------------- *//**
 *
 * @file glm_family.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief family class: distribution families
 */
namespace madlib {
namespace modules {
namespace regress {
class family
{
public:
    typedef double (*LINKFUN)(double);
    typedef double (*LINKINV)(double);
    family(const std::string f, const std::string l);

    LINKFUN linkfun;
    LINKINV linkinv;
protected:
    // link functions
    static double linkfun_gaussian_identical(double);
    static double linkfun_gaussian_inverse(double);
    static double linkfun_gaussian_log(double);

    // linkinv functions
    static double linkinv_gaussian_identical(double);
    static double linkinv_gaussian_inverse(double);
    static double linkinv_gaussian_log(double);
};
} // regress
} // modules
} // madlib

/**
 * @brief Gaussian family (conjugate-gradient step): Final function
 */
DECLARE_UDF(regress, gaussian)

