#include <string>
#include <boost/lexical_cast.hpp>

//Boost.Multiprecision mpfr_float
#include <boost/multiprecision/mpfr.hpp>

//Boost.Multiprecision mpc_complex
#include <complex>
#include <boost/multiprecision/mpc.hpp>

//for parallel implementation
#include <boost/mpi.hpp>
#include <boost/thread.hpp>

//Boost.Math headers
#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/factorials.hpp>

//Boost.Serialization headers
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/version.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/array.hpp>
#include <stdio.h> // used in creating File object

//Boost.Ublas headers
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/triangular.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/vector_proxy.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/io.hpp>

//for serialization and miscellaneous functionalities
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include<stdlib.h>
#include<time.h>

#define digits 1500

using namespace boost::archive;
using namespace boost::serialization;
using namespace boost::multiprecision;
using namespace boost::numeric::ublas;
using namespace std::chrono;


//define new arbitrary precision complex and real types
//using realtype = number<mpfr_float_backend<digits, allocate_stack>>;
using realtype = number<mpfr_float_backend<digits>>; //will not interoperate (*) with mpc types if allocated on stack
using complextype = number<mpc_complex_backend<digits>>;

//serialization code for mpfr_float

#define MPFR_BUFFER_SIZE 1510

    namespace boost {
        namespace serialization {
            template<class Archive>
            void save(Archive& ar, const realtype& x, const boost::serialization::version_type&) {
                static char buffer[MPFR_BUFFER_SIZE];
                FILE* fid = fmemopen(buffer, MPFR_BUFFER_SIZE, "wb+");
                mpfr_fpif_export(fid, const_cast<mpfr_ptr>(x.backend().data()));
                fseek(fid, 0L, SEEK_END);
                long length = ftell(fid);
                ar& length;
                ar& boost::serialization::make_array(buffer, length);
                fclose(fid);
            }

            template<class Archive>
            void load(Archive& ar, realtype& x, const boost::serialization::version_type&) {
                static char buffer[MPFR_BUFFER_SIZE];
                long length = 0;

                ar& length;
                ar& boost::serialization::make_array(buffer, length);

                FILE* fid = fmemopen(buffer, length, "r");
                mpfr_fpif_import(x.backend().data(), fid);
                fclose(fid);
            }

            template<class Archive>
            inline void serialize(Archive& ar, realtype& t, const unsigned int file_version) {
                split_free(ar, t, file_version);
            }
        }
    }

namespace mpi = boost::mpi;
namespace math = boost::math;

int i,j,k,m,n, l;
int betas = 35; // number of betas to test
const int s = 150, b = 9; // l*(b+1)  = d+1
const int d = s*(b+1) - 1;
const realtype Pi = boost::math::constants::pi<realtype>();
const realtype nu = -realtype(1)/realtype(2);
const realtype sine1 = Pi/( sin(Pi * nu) );

const realtype cosine = cos( (Pi * nu)/realtype(2) ) ;
const realtype sine = sin( (Pi * nu)/realtype(2) ) ;

const complextype I{0,1};

//ublas vectors
vector<complextype> term(d+1), term1(d+1);
vector<realtype> factor (d+1);
vector<realtype> constants(d+1);
vector<realtype> factor2 (d+1);

int main()
{

    auto start = high_resolution_clock::now();


    vector<realtype> beta (betas);
    for (i = 0; i < betas-2; ++i)
    {
        beta(i) = pow( realtype(10), realtype( i - 5 ) );
    }

    beta(betas-2) = realtype(1)/realtype(5);
    beta(betas-1) = realtype(4);

    //--------------------factor--------------------------------------------

    /* Here we compute the all the relevant factors appearing repeatedly in the
     each iteration of the multiple sum in the definition of the function g(x).
     This factors are computed here once and for all and stored later for use. This saves a lot of computing time. */

    for (i = 0; i < factor.size (); ++i)
    {
        factor(i) = realtype(1) / boost::math::factorial < realtype > ( d - i );
        factor2(i) =  realtype(1) /  pow ( math::factorial < realtype > ( i ), realtype(2) );
    }

    //-------------read-in the constants------------------------------------
    std::ifstream infile;
    std::string st1;
    infile.open( "/scratch3/chris.tica/SEXTIC/1_5k_mom_1_5kdig/Constants/Constant.txt" );
    for ( i=0; i < constants.size(); ++i )
    {
        std::getline(infile, st1);
        constants(i) = realtype(st1);
    }

    infile.close();

    //----------------------------------------------------------------------------------

    vector<complextype> fifthter(betas);

    std::ofstream outfile1;
    outfile1.open("/scratch3/chris.tica/SEXTIC/1_5k_mom_1_5kdig/results/FIFTH.txt",std::ios_base::out);
    outfile1.precision(digits);

    for ( i = 0; i < betas ; ++i )
    {
        for (  l = 0; l < d + 1; ++l )
        {
            term(l) = pow(-I / sqrt(beta(i) ), realtype(l) ) * factor2(l);
        }

        for( m = 0; m < d+1; ++m )
        {
            term1(m) = inner_prod( project( term, range(0, m+1) ), project( factor, range (d-m , d + 1) ) );
        }

        fifthter(i) =  inner_prod( constants, term1 ); // the summation part of g(x), ie w/o the exponential

        outfile1 << std::scientific <<  ( sine1 * pow( beta(i), (realtype(1)+nu) / realtype(2) ) ) * (cosine*imag( exp( -I /( realtype(2)*sqrt(beta(i) ) ) ) * fifthter(i) ) +
                         sine * real( exp( -I /( realtype(2) * sqrt(beta(i) ) ) ) * fifthter(i) ) )   << std::endl;

    }


    //print the total run-time
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<minutes>(stop - start);
    std::cout << duration.count() << "minutes" << std::endl;

    return 0;

}