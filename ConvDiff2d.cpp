#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <iostream>
#include <vector>
#include <cmath>
#include <memory>
#include <Eigen/IterativeLinearSolvers>
#include <stdio.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <SDL/SDL.h>
#include <queue>
#include <unsupported/Eigen/FFT>

const double PI = 3.141592653589793238462;

typedef Eigen::SparseMatrix<double> SpMat;
typedef Eigen::VectorXd Vec;
typedef Eigen::Triplet<double> Trip;
typedef Eigen::MatrixXd Mat;

// precomputable quantities
const int POLYMAX = 10;
double weights[22];
double coords[22];
double normLegendreDerivProducts[POLYMAX+1][POLYMAX+1];
double normLegendreAltProducts[POLYMAX+1][POLYMAX+1];
double normLegendreLeftVals[POLYMAX+1];
double normLegendreRightVals[POLYMAX+1];
double normLegendreDerivLeftVals[POLYMAX+1];
double normLegendreDerivRightVals[POLYMAX+1];

class ConvDiff
{
private:
	int N;
	int K;
	int dof;
	double L;
	double epsilon = -1.0;
	double diffconst = 1.0;
	double sigma0;
	double beta0 = 1.0;
	SpMat A;
	SpMat UXP;
	SpMat UXM;
	SpMat UYP;
	SpMat UYM;
	Vec rhs;
	double ux = 0.0;
	double uy = 0.0;
	Eigen::BiCGSTAB<SpMat,Eigen::IncompleteLUT<double>> solver;
	Vec phi;
	void BuildMatA();
	void BuildMatUXP();
	void BuildMatUXM();
	void BuildMatUYP();
	void BuildMatUYM();
	void BuildRHS();
public:
	ConvDiff(int N,int K,double L) : N(N), K(K), L(L), dof(((N*N*(K+1)*(K+2))/2)), sigma0((K+1)*(K+2)*4+1)
	{}
	void init()
	{
		A.resize(dof,dof);
		UXP.resize(dof,dof);
		UXM.resize(dof,dof);
		UYP.resize(dof,dof);
		UYM.resize(dof,dof);
		rhs.resize(dof);
		phi.resize(dof);
		BuildMatA();
		BuildMatUXP();
		BuildMatUXM();
		BuildMatUYP();
		BuildMatUYM();
		BuildRHS();
	}
	void reinit(int N, int K, double L)
	{
		this->N = N;
		this->K = K;
		this->L = L;
		dof = ((N*N*(K+1)*(K+2))/2);
		sigma0 = (K+1)*(K+2)*4+1;
		init();
	}
	inline int idx(int ix, int iy, int px, int py) { return (((K+1)*(K+2)*(N*((ix+N)%N)+(iy+N)%N))/2 + ((px+py)*(px+py+1))/2 + px); }
	double Eval(double x, double y);
	void SetU(double ux, double uy) { this->ux = ux; this->uy = uy; }
	double SolResid();
	double Solve()
	{
		SpMat U = (ux>0.0?ux:0.0)*UXP + (ux<0.0?ux:0.0)*UXM + (uy>0.0?uy:0.0)*UYP + (uy<0.0?uy:0.0)*UYM;
		SpMat R = A+U;
		R.makeCompressed();
		solver.compute(R);
		phi = solver.solve(rhs);
		return (R*phi-rhs).norm();
	}
};


void MakeWeights()
{
	weights[0] = 0.1392518728556320;
	coords[0] = -0.0697392733197222;
	weights[1] = 0.1392518728556320;
	coords[1] = 0.0697392733197222;
	weights[2] = 0.1365414983460152;
	coords[2] = -0.2078604266882213;
	weights[3] = 0.1365414983460152;
	coords[3] = 0.2078604266882213;
	weights[4] = 0.1311735047870624;
	coords[4] = -0.3419358208920842;
	weights[5] = 0.1311735047870624;
	coords[5] = 0.3419358208920842;
	weights[6] = 0.1232523768105124;
	coords[6] = -0.4693558379867570;
	weights[7] = 0.1232523768105124;
	coords[7] = 0.4693558379867570;
	weights[8] = 0.1129322960805392;
	coords[8] = -0.5876404035069116;
	weights[9] = 0.1129322960805392;
	coords[9] = 0.5876404035069116;
	weights[10] = 0.1004141444428810;
	coords[10] = -0.6944872631866827;
	weights[11] = 0.1004141444428810;
	coords[11] = 0.6944872631866827;
	weights[12] = 0.0859416062170677;
	coords[12] = -0.7878168059792081;
	weights[13] = 0.0859416062170677;
	coords[13] = 0.7878168059792081;
	weights[14] = 0.0697964684245205;
	coords[14] = -0.8658125777203002;
	weights[15] = 0.0697964684245205;
	coords[15] = 0.8658125777203002;
	weights[16] = 0.0522933351526833;
	coords[16] = -0.9269567721871740;
	weights[17] = 0.0522933351526833;
	coords[17] = 0.9269567721871740;
	weights[18] = 0.0337749015848142;
	coords[18] = -0.9700604978354287;
	weights[19] = 0.0337749015848142;
	coords[19] = 0.9700604978354287;
	weights[20] = 0.0146279952982722;
	coords[20] = -0.9942945854823992;
	weights[21] = 0.0146279952982722;
	coords[21] = 0.9942945854823992;
}

double LegendreEval(int p, double y)
{
	if(p == 0) return 1.0;
	if(p == 1) return y;
	double prev = 1.0;
	double cur = y;
	for(int n = 1; n < p; n++)
	{
		double next = ((2.0*n+1.0)*y*cur - n*prev)/(n+1.0);
		prev = cur;
		cur = next;
	}
	return cur;
}

double LegendreDerivEval(int p, double y)
{
	double val = 0.0;
	for(int n = 0; n < p; n++)
	{
		val = (n+1.0)*LegendreEval(n,y) + y*val;
	}
	return val;
}

double LegendreL2Norm(int p)
{
	return std::pow(2.0/(2.0*p+1.0),0.5);
}



double LegendreEvalNorm(int p, double y)
{
	return LegendreEval(p,y) / LegendreL2Norm(p);
}

double LegendreDerivEvalNorm(int p, double y)
{
	return LegendreDerivEval(p,y) / LegendreL2Norm(p);
}

void MakeLegendreEndpointVals()
{
	for(int p = 0; p < POLYMAX+1; p++)
	{
		normLegendreLeftVals[p] = LegendreEvalNorm(p,-1.0);
		normLegendreRightVals[p] = LegendreEvalNorm(p,1.0);
		normLegendreDerivLeftVals[p] = LegendreDerivEvalNorm(p,-1.0);
		normLegendreDerivRightVals[p] = LegendreDerivEvalNorm(p,1.0);
	}
}

void MakeLegendreDerivProducts()
{
	for(int p = 0; p < POLYMAX+1; p++)
	{
		for(int q = 0; q < POLYMAX+1; q++)
		{
			double res = 0.0;
			for(int k = 0; k < 22; k++)
			{
				res += weights[k] * LegendreDerivEvalNorm(p,coords[k]) * LegendreDerivEvalNorm(q,coords[k]);
			}
			normLegendreDerivProducts[p][q] = res;
		}
	}
}

void MakeLegendreAltProducts()
{
	for(int p = 0; p < POLYMAX+1; p++)
	{
		for(int q = 0; q < POLYMAX+1; q++)
		{
			double res = 0.0;
			for(int k = 0; k < 22; k++)
			{
				res += weights[k] * LegendreEvalNorm(p,coords[k]) * LegendreDerivEvalNorm(q,coords[k]);
			}
			normLegendreAltProducts[p][q] = res;
		}
	}
}

void ConvDiff::BuildMatA()
{
	double h = L/N;
	double hbeta0 = std::pow(h,beta0);
	std::vector<Trip> elems;

	// Diagonal blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val += diffconst * std::pow(2.0/h,2) * normLegendreDerivProducts[px][qx] * (py == qy ? 1.0 : 0.0);
					val += diffconst * std::pow(2.0/h,2) * (px == qx ? 1.0 : 0.0) * normLegendreDerivProducts[py][qy];
					
					// East
					val += diffconst * (2.0/h) * (2.0/h) * (-0.5) * (py == qy ? 1.0 : 0.0)
						* normLegendreRightVals[qx] * normLegendreDerivRightVals[px];
					val -= (2.0/h) * (2.0/h) *(-1.0)* epsilon * 0.5 * (py == qy ? 1.0 : 0.0)
						* normLegendreRightVals[px] * normLegendreDerivRightVals[qx];
					val += (2.0/h) * (sigma0/hbeta0) * ( normLegendreRightVals[px]*normLegendreRightVals[qx] )
						* (py == qy ? 1.0 : 0.0);
					
					// West
					val += diffconst * (2.0/h) *(2.0/h) * (0.5) * (py == qy ? 1.0 : 0.0)
						* normLegendreLeftVals[qx] * normLegendreDerivLeftVals[px];
					val -= (2.0/h) *(2.0/h) * epsilon * 0.5 * (py == qy ? 1.0 : 0.0)
						* normLegendreLeftVals[px] * normLegendreDerivLeftVals[qx];
					val += (2.0/h) * (sigma0/hbeta0) * ( normLegendreLeftVals[qx]*normLegendreLeftVals[px] )
						* (py == qy ? 1.0 : 0.0);
					

					// North
					val += diffconst * (2.0/h) *(2.0/h) * (-0.5) * (px == qx ? 1.0 : 0.0)
						* normLegendreRightVals[qy] * normLegendreDerivRightVals[py];
					val -= (2.0/h) * (-1.0)*(2.0/h) * epsilon * 0.5 * (px == qx ? 1.0 : 0.0)
						* normLegendreRightVals[py] * normLegendreDerivRightVals[qy];
					val += (2.0/h) * (sigma0/hbeta0) * ( normLegendreRightVals[py]*normLegendreRightVals[qy] )
						* (px == qx ? 1.0 : 0.0);
					
					// South
					val += diffconst * (2.0/h) *(2.0/h) * (0.5) * (px == qx ? 1.0 : 0.0)
						* normLegendreLeftVals[qy] * normLegendreDerivLeftVals[py];
					val -= (2.0/h) * epsilon * 0.5 *(2.0/h) * (px == qx ? 1.0 : 0.0)
						* normLegendreLeftVals[py] * normLegendreDerivLeftVals[qy];
					val += (2.0/h) * (sigma0/hbeta0) * ( normLegendreLeftVals[qy]*normLegendreLeftVals[py] )
						* (px == qx ? 1.0 : 0.0);
					
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
							if(idxv == 0 && px == 0 && qx == 0)
							{
								Trip t1(idxv,idxphi,1.0);
								elems.push_back(t1);
							}
						}
					}
				}
			}
		}
	}

	// East blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val += diffconst * (2.0/h) *(2.0/h) * (-0.5) * (py == qy ? 1.0 : 0.0)
						* normLegendreRightVals[qx] * normLegendreDerivLeftVals[px];
					val -= (2.0/h) * epsilon *(2.0/h) * 0.5 * (py == qy ? 1.0 : 0.0)
						* normLegendreLeftVals[px] * normLegendreDerivRightVals[qx];
					val += (2.0/h) * (-1.0 * sigma0/hbeta0) * ( normLegendreLeftVals[px]*normLegendreRightVals[qx] )
						* (py == qy ? 1.0 : 0.0);
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix+1,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	// West blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val += diffconst * (2.0/h) * (0.5) *(2.0/h) * (py == qy ? 1.0 : 0.0)
						* normLegendreLeftVals[qx] * normLegendreDerivRightVals[px];
					val -= (2.0/h) * (-1.0) * epsilon * 0.5 *(2.0/h) * (py == qy ? 1.0 : 0.0)
						* normLegendreRightVals[px] * normLegendreDerivLeftVals[qx];
					val += (2.0/h) * (-1.0 * sigma0/hbeta0) * ( normLegendreLeftVals[qx]*normLegendreRightVals[px] )
						* (py == qy ? 1.0 : 0.0);
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix-1,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	// North blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val += diffconst * (2.0/h) * (-0.5) *(2.0/h) * (px == qx ? 1.0 : 0.0)
						* normLegendreRightVals[qy] * normLegendreDerivLeftVals[py];
					val -= (2.0/h) * epsilon * 0.5 *(2.0/h) * (px == qx ? 1.0 : 0.0)
						* normLegendreLeftVals[py] * normLegendreDerivRightVals[qy];
					val += (2.0/h) * (-1.0 * sigma0/hbeta0) * ( normLegendreLeftVals[py]*normLegendreRightVals[qy] )
						* (px == qx ? 1.0 : 0.0);
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy+1,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	// South blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val += diffconst * (2.0/h) * (0.5) *(2.0/h) * (px == qx ? 1.0 : 0.0)
						* normLegendreLeftVals[qy] * normLegendreDerivRightVals[py];
					val -= (2.0/h) * (-1.0) * epsilon * 0.5 *(2.0/h) * (px == qx ? 1.0 : 0.0)
						* normLegendreRightVals[py] * normLegendreDerivLeftVals[qy];
					val += (2.0/h) * (-1.0 * sigma0/hbeta0) * ( normLegendreLeftVals[qy]*normLegendreRightVals[py] )
						* (px == qx ? 1.0 : 0.0);
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy-1,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	A.setFromTriplets(elems.begin(),elems.end());
}



void ConvDiff::BuildMatUXP()
{
	double h = L/N;
	std::vector<Trip> elems;

	// Diagonal blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -= (2.0/h) *normLegendreAltProducts[px][qx] * (py == qy ? 1.0 : 0.0);
					
					val -= (2.0/h) * (-1.0) * normLegendreRightVals[px]*normLegendreRightVals[qx]
						 * ( py == qy ? 1.0 : 0.0 );

					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	// West blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -= (2.0/h) * normLegendreLeftVals[qx]*normLegendreRightVals[px]
						 * ( py == qy ? 1.0 : 0.0 );
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix-1,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	UXP.setFromTriplets(elems.begin(),elems.end());
}


void ConvDiff::BuildMatUXM()
{
	double h = L/N;
	std::vector<Trip> elems;

	// Diagonal blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -= (2.0/h) *normLegendreAltProducts[px][qx] * (py == qy ? 1.0 : 0.0);
					val -= (2.0/h) * normLegendreLeftVals[qx]*normLegendreLeftVals[px]
						 * ( py == qy ? 1.0 : 0.0 );

					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	// East blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -= (2.0/h) * (-1.0) * normLegendreLeftVals[px]*normLegendreRightVals[qx]
						 * ( py == qy ? 1.0 : 0.0 );
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix+1,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	UXM.setFromTriplets(elems.begin(),elems.end());
}



void ConvDiff::BuildMatUYP()
{
	double h = L/N;
	std::vector<Trip> elems;

	// Diagonal blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -=(2.0/h) * (px == qx ? 1.0 : 0.0) * normLegendreAltProducts[py][qy];
					val -= (2.0/h) * (-1.0) * normLegendreRightVals[py]*normLegendreRightVals[qy]
						 * ( px == qx ? 1.0 : 0.0 );

					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	// South blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -= (2.0/h) * normLegendreLeftVals[qy]*normLegendreRightVals[py]
						 * ( px == qx ? 1.0 : 0.0 );
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy-1,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	UYP.setFromTriplets(elems.begin(),elems.end());
}


void ConvDiff::BuildMatUYM()
{
	double h = L/N;
	std::vector<Trip> elems;

	// Diagonal blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -= (2.0/h) *(px == qx ? 1.0 : 0.0) * normLegendreAltProducts[py][qy];
					val -= (2.0/h) * normLegendreLeftVals[qy]*normLegendreLeftVals[py]
						 * ( px == qx ? 1.0 : 0.0 );

					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	// North blocks
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			for(int qx = 0; qx < K+1; qx++)
			{
				for(int qy = 0; qy < K+1-qx; qy++)
				{
					double val = 0.0;
					val -= (2.0/h) * (-1.0) * normLegendreLeftVals[py]*normLegendreRightVals[qy]
						 * ( px == qx ? 1.0 : 0.0 );
					for(int ix = 0; ix < N; ix++)
					{
						for(int iy = 0; iy < N; iy++)
						{
							int idxv = idx(ix,iy,qx,qy);
							int idxphi = idx(ix,iy+1,px,py);
							Trip t(idxv,idxphi,val);
							if(idxv != 0) elems.push_back(t);
						}
					}
				}
			}
		}
	}

	UYM.setFromTriplets(elems.begin(),elems.end());
}




double PeriodicGaussian(double x, double y, double r)
{
	double val = 0.0;
	for(int i = -2; i <= 2; i++)
	{
		for(int j = -2; j <= 2; j++)
		{
			val += std::exp(-0.5*std::pow((x-1.0*i)/r,2)-0.5*std::pow((y-1.0*j)/r,2));
		}
	}
	return val;
}

double EvalRHS(double x, double y)
{ 
	return PeriodicGaussian(x-0.2,y-0.8,0.15) - PeriodicGaussian(x-0.8,y-0.2,0.15);
}

void ConvDiff::BuildRHS()
{
	double h = L/N;
	for(int ix = 0; ix < N; ix++)
	{
		for(int iy = 0; iy < N; iy++)
		{
			double xc = (ix+0.5)*h;
			double yc = (iy+0.5)*h;
			for(int px = 0; px < K+1; px++)
			{
				for(int py = 0; py < K+1-px; py++)
				{
					double val = 0.0;
					for(int j = 0; j < 22; j++)
					{
						for(int k = 0; k < 22; k++)
						{
							val += weights[j]*weights[k]
								* LegendreEvalNorm(px,coords[j])
								* LegendreEvalNorm(py,coords[k])
								* EvalRHS((xc+coords[j]*(h/2.0))/L, (yc+coords[k]*(h/2.0))/L);
						}
					}
					rhs(idx(ix,iy,px,py)) = val;
				}
			}
		}
	}
	rhs(0) = 0.0;
}

double ConvDiff::Eval(double x, double y)
{
	if(x < 0.0) return Eval(x+L,y);
	if(x>L) return Eval(x-L,y);
	if(y<0.0) return Eval(x,y+L);
	if(y > L) return Eval(x,y-L);
	double h = L/N;
	int ix = x/h;
	int iy = y/h;
	double val = 0.0;
	double xc = (ix+0.5)*h;
	double yc = (iy+0.5)*h;
	for(int px = 0; px < K+1; px++)
	{
		for(int py = 0; py < K+1-px; py++)
		{
			val += phi(idx(ix,iy,px,py)) * LegendreEvalNorm(px,(x-xc)*(2.0/h)) * LegendreEvalNorm(py,(y-yc)*(2.0/h));
		}
	}
	return val;
}

double ConvDiff::SolResid()
{
	int sp = 21;
	int numpts = sp*sp;
	double resid = 0.0;
	double sizeRHS = 0.0;
	double h = L/sp;
	for(int i = 0; i < sp; i++)
	{
		for(int j = 0; j < sp; j++)
		{
			double xx = (0.5+i)*h;
			double yy = (0.5+j)*h;
			double val = 0.0;
			val -= diffconst*( -Eval(xx+4.0*h,yy)/560.0 + Eval(xx+3.0*h,yy)*8.0/315.0  -Eval(xx+2.0*h,yy)/5.0+Eval(xx+h,yy)*8.0/5.0+Eval(xx-h,yy)*8.0/5.0-Eval(xx-2.0*h,yy)/5.0 + Eval(xx-3.0*h,yy)*8.0/315.0 - Eval(xx-4.0*h,yy)/560.0 - Eval(xx,yy+4.0*h)/560.0+Eval(xx,yy+3.0*h)*8.0/315.0 -Eval(xx,yy+2.0*h)/5.0+Eval(xx,yy+h)*8.0/5.0+Eval(xx,yy-h)*8.0/5.0-Eval(xx,yy-2.0*h)/5.0 + Eval(xx,yy-3.0*h)*8.0/315.0 - Eval(xx,yy-4.0*h)/560.0 - Eval(xx,yy)*2.0*205.0/72.0 )/(h*h);
			val += ux * (-Eval(xx+4.0*h,yy)/280.0+Eval(xx+3.0*h,yy)*4.0/105.0-Eval(xx+2.0*h,yy)/5.0+Eval(xx+h,yy)*4.0/5.0-Eval(xx-h,yy)*4.0/5.0+Eval(xx-2.0*h,yy)/5.0-Eval(xx-3.0*h,yy)*4.0/105.0+Eval(xx-4.0*h,yy)/280.0)/(h);
			val += uy * (-Eval(xx,yy+4.0*h)/280.0+Eval(xx,yy+3.0*h)*4.0/105.0-Eval(xx,yy+2.0*h)/5.0+Eval(xx,yy+h)*4.0/5.0-Eval(xx,yy-h)*4.0/5.0+Eval(xx,yy-2.0*h)/5.0-Eval(xx,yy-3.0*h)*4.0/105.0+Eval(xx,yy-4.0*h)/280.0)/(h);
			val -= EvalRHS(xx/L,yy/L);
			resid += std::pow(val,2);
			sizeRHS += std::pow(EvalRHS(xx/L,yy/L),2);
		}
	}
	resid = std::pow(resid/numpts,0.5);
	sizeRHS = std::pow(sizeRHS/numpts,0.5);
	return resid/sizeRHS;
}

void FFT2D(Mat& input,Mat& outputRe,Mat& outputIm)
{
	Eigen::FFT<double> fft;
	int rows = input.rows();
	int cols = input.cols();
	for(int i = 0; i < rows; i++)
	{
		std::vector<double> row;
		for(int j = 0; j < cols; j++) row.push_back(input(i,j));
		std::vector<std::complex<double>> freqs;
		fft.fwd(freqs,row);
		for(int j = 0; j < cols; j++)
		{
			outputRe(i,j) = freqs[j].real();
			outputIm(i,j) = freqs[j].imag();
		}
	}
	for(int j = 0; j < cols; j++)
	{
		std::vector<double> colRe;
		std::vector<double> colIm;
		for(int i = 0; i < rows; i++)
		{
			colRe.push_back(outputRe(i,j));
			colIm.push_back(outputIm(i,j));
		}
		std::vector<std::complex<double>> freqsRe;
		std::vector<std::complex<double>> freqsIm;
		fft.fwd(freqsRe,colRe);
		fft.fwd(freqsIm,colIm);
		for(int i = 0; i < rows; i++)
		{
			outputRe(i,j) = freqsRe[i].real() - freqsIm[i].imag();
			outputIm(i,j) = freqsRe[i].imag() + freqsIm[i].real();
		}
	}
}

void IFFT2D(Mat& inputRe, Mat& inputIm,Mat& outputRe,Mat& outputIm)
{
	Eigen::FFT<double> fft;
	int rows = inputRe.rows();
	int cols = inputRe.cols();
	for(int i = 0; i < rows; i++)
	{
		std::vector<std::complex<double>> row;
		for(int j = 0; j < cols; j++)
		{
			std::complex<double> elem(inputRe(i,j),inputIm(i,j));
			row.push_back(elem);
		}
		std::vector<std::complex<double>> res;
		fft.inv(res,row);
		for(int j = 0; j < cols; j++)
		{
			outputRe(i,j) = res[j].real();
			outputIm(i,j) = res[j].imag();
		}
	}
	for(int j = 0; j < cols; j++)
	{
		std::vector<std::complex<double>> col;
		for(int i = 0; i < rows; i++)
		{
			std::complex<double> elem(outputRe(i,j),outputIm(i,j));
			col.push_back(elem);
		}
		std::vector<std::complex<double>> res;
		fft.inv(res,col);
		for(int i = 0; i < rows; i++)
		{
			outputRe(i,j) = res[i].real();
			outputIm(i,j) = res[i].imag();
		}
	}
}

extern "C" {

const int NUMPIXELS = 693;

Mat dispRe(NUMPIXELS,NUMPIXELS);
Mat dispIm(NUMPIXELS,NUMPIXELS);
Mat dispTemp(11,11);
Mat dispTempFFTRe(11,11);
Mat dispTempFFTIm(11,11);
Mat dispFFTRe(NUMPIXELS,NUMPIXELS);
Mat dispFFTIm(NUMPIXELS,NUMPIXELS);

double len = 1.0;
SDL_Surface *screen;
ConvDiff convDiff(11,1,len);
ConvDiff convDiffHigh(3,10,len);
std::queue<int> workQueue;
bool convDiffInited(false);
bool convDiffHighInited(false);
bool mouseIsDown(false);
bool touchIsStarted(false);

double getVelocityX(long targetX)
{
	return (targetX-350.0)/2;
}


double getVelocityY(long targetY)
{
	return (targetY-350.0)/2;
}

void mouse_update(const EmscriptenMouseEvent *e)
{
	double ux = getVelocityX(e->targetX);
	double uy = getVelocityY(e->targetY);
	convDiff.SetU(ux,uy);
	convDiffHigh.SetU(ux,uy);
}

void touch_update(const EmscriptenTouchEvent *e)
{
	if(e->numTouches > 0)
	{
		long targetX = 0;
		long targetY = 0;
		for(int i = 0; i < e->numTouches; i++)
		{
			targetX += e->touches[i].targetX;
			targetY += e->touches[i].targetY;
		}
		targetX /= e->numTouches;
		targetY /= e->numTouches;
		double ux = getVelocityX(targetX);
		double uy = getVelocityY(targetY);
		convDiff.SetU(ux,uy);
		convDiffHigh.SetU(ux,uy);
	}
}

const int NUMLEVELS = 13;

int getColorIndex(double val)
{
	return std::floor( (NUMLEVELS-1) * ((val+0.0001)/1.0002) );
}

double getLambda(double val, int colorIndex)
{
	return (val-colorIndex/(1.0*NUMLEVELS-1.0))*(1.0*NUMLEVELS-1.0);
}

double red(double lambda, int colorIndex, double low = 1.0, double high = 254.0)
{
	return 0.47<lambda && lambda<0.53 ? high : low;
}

double green(double lambda, int colorIndex, double low = 1.0, double high = 254.0)
{
	return 0.47<lambda && lambda<0.53 ? high : low;
}

double blue(double lambda, int colorIndex, double low = 1.0, double high = 254.0)
{
	return 0.47<lambda && lambda<0.53 ? high : low;
}

void repaintHigh()
{
	double maxphi = convDiffHigh.Eval(0.0, 0.0);
	double minphi = convDiffHigh.Eval(0.0,0.0);
	for(int i = 0; i < 100; i++)
	{
		for(int j = 0; j < 100; j++)
		{
			double val = convDiffHigh.Eval(len*(1.0*j)/100,len*(1.0*i)/100);
			if(val > maxphi) maxphi = val;
			if(val < minphi) minphi = val;
		}
	}

	if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	for (int i = 0; i < NUMPIXELS; i++) {
		for (int j = 0; j < NUMPIXELS; j++) {
			double val = (convDiffHigh.Eval(len*(1.0*j)/NUMPIXELS,len*(1.0*i)/NUMPIXELS)-minphi)/(maxphi-minphi);
			val = 0.97*(val-0.5)+0.5;
			int colorIndex = getColorIndex(val);
			double lambda = getLambda(val,colorIndex);
			double valr = red(lambda,colorIndex)*255.0;
			double valg = green(lambda,colorIndex)*255.0;
			double valb = blue(lambda,colorIndex)*255.0;
			*((Uint32*)screen->pixels + i * NUMPIXELS + j) = SDL_MapRGBA(screen->format, (int)valr, (int)valg, (int)valb, 255);
		}
	}
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_Flip(screen); 
}


void repaintLow()
{

	for(int i = 0; i < 11; i++)
	{
		for(int j = 0; j < 11; j++)
		{
			dispTemp(i,j) = (convDiff.Eval(len*(1.0*(j+0.5))/11,len*(1.0*(i+0.5))/11));
		}
	}

	FFT2D(dispTemp,dispTempFFTRe,dispTempFFTIm);

	for(int i = 0; i < 6; i++)
	{
		for(int j = 0; j < 6; j++)
		{
			dispFFTRe(i,j) = dispTempFFTRe(i,j);
			dispFFTIm(i,j) = dispTempFFTIm(i,j);
		}
	}

	for(int i = 1; i < 6; i++)
	{
		for(int j = 0; j < 6; j++)
		{
			dispFFTRe(NUMPIXELS-i,j) = dispTempFFTRe(11-i,j);
			dispFFTIm(NUMPIXELS-i,j) = dispTempFFTIm(11-i,j);
		}
	}

	for(int i = 0; i < 6; i++)
	{
		for(int j = 1; j < 6; j++)
		{
			dispFFTRe(i,NUMPIXELS-j) = dispTempFFTRe(i,11-j);
			dispFFTIm(i,NUMPIXELS-j) = dispTempFFTIm(i,11-j);
		}
	}

	for(int i = 1; i < 6; i++)
	{
		for(int j = 1; j < 6; j++)
		{
			dispFFTRe(NUMPIXELS-i,NUMPIXELS-j) = dispTempFFTRe(11-i,11-j);
			dispFFTIm(NUMPIXELS-i,NUMPIXELS-j) = dispTempFFTIm(11-i,11-j);
		}
	}

	IFFT2D(dispFFTRe,dispFFTIm,dispRe,dispIm);

	double maxphi = dispRe.maxCoeff();
	double minphi = dispRe.minCoeff();

	

	if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	for (int i = 0; i < NUMPIXELS; i++) {
		for (int j = 0; j < NUMPIXELS; j++) {
			double val = (dispRe((i+NUMPIXELS-32)%NUMPIXELS,(j+NUMPIXELS-32)%NUMPIXELS)-minphi)/(maxphi-minphi);
			val = 0.97*(val-0.5)+0.5;
			int colorIndex = getColorIndex(val);
			double lambda = getLambda(val,colorIndex);
			double valr = red(lambda,colorIndex,1.0,175.0)*255.0;
			double valg = green(lambda,colorIndex,1.0,175.0)*255.0;
			double valb = blue(lambda,colorIndex,1.0,175.0)*255.0;
			*((Uint32*)screen->pixels + i * NUMPIXELS + j) = SDL_MapRGBA(screen->format, (int)valr, (int)valg, (int)valb, 255);
		}
	}
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_Flip(screen); 
}


EM_BOOL mouseclick_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	mouse_update(e);
	workQueue.push(0);
	workQueue.push(1);
	mouseIsDown = false;
	return 1;
}

EM_BOOL mouseleave_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	mouseIsDown = false;
	return 1;
}

EM_BOOL mouseup_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	mouseIsDown = false;
	return 1;
}

EM_BOOL mousedown_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	mouseIsDown = true;
	mouse_update(e);
	workQueue.push(0);
	return 1;
}

EM_BOOL mousemove_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	if(mouseIsDown)
	{
		mouse_update(e);
		workQueue.push(0);
	}
	return 1;
}

EM_BOOL touchmove_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
{
	if(touchIsStarted)
	{
		touch_update(e);
		workQueue.push(0);
	}
	return 1;
}

EM_BOOL touchstart_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
{
	touchIsStarted = true;
	touch_update(e);
	workQueue.push(0);
	return 1;
}

EM_BOOL touchend_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
{
	touchIsStarted = false;
	touch_update(e);
	workQueue.push(0);
	workQueue.push(1);
	return 1;
}

EM_BOOL touchcancel_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
{
	touchIsStarted = false;
	return 1;
}

void EMSCRIPTEN_KEEPALIVE rebuild(int N, int K)
{
	convDiffHigh.reinit(N,K > POLYMAX ? POLYMAX : K,len);
	workQueue.push(1);
}

int n = 0;
void init()
{
	if(n < 5)
	{
		n++;
		return;
	}
	if(workQueue.empty()) return;

	int workItem = workQueue.front();
	workQueue.pop();

	if(workItem < 0) return;

	if(workItem == 0)
	{
		if(!convDiffInited)
		{
			convDiff.init();
			convDiffInited = true;
		}
		convDiff.Solve();
		repaintLow();
	}
	else
	{
		if(!convDiffHighInited)
		{
			convDiffHigh.init();
			convDiffHighInited = true;
		}
		
		double matResid = convDiffHigh.Solve();
		double solResid = convDiffHigh.SolResid();
		printf("matrix residual %3.2e, spatial residual %3.2e\n", matResid, solResid);
		repaintHigh();
	}
}


int main(int argc, char ** argv)
{
	MakeWeights();
	MakeLegendreDerivProducts();
	MakeLegendreAltProducts();
	MakeLegendreEndpointVals();

	workQueue.push(0);
	workQueue.push(1);
	
	SDL_Init(SDL_INIT_VIDEO);
	screen = SDL_SetVideoMode(NUMPIXELS, NUMPIXELS, 32, SDL_SWSURFACE);

	emscripten_set_click_callback("canvas", 0, 1, mouseclick_callback);
	emscripten_set_mousedown_callback("canvas", 0, 1, mousedown_callback);
	emscripten_set_mouseup_callback("canvas", 0, 1, mouseup_callback);
	emscripten_set_mousemove_callback("canvas", 0, 1, mousemove_callback);
	emscripten_set_mouseleave_callback("canvas", 0, 1, mouseleave_callback);
	emscripten_set_touchstart_callback("canvas", 0, 1, touchstart_callback);
	emscripten_set_touchend_callback("canvas", 0, 1, touchend_callback);
	emscripten_set_touchcancel_callback("canvas", 0, 1, touchcancel_callback);
	emscripten_set_touchmove_callback("canvas", 0, 1, touchmove_callback);


	emscripten_set_main_loop(init,0,0);
}

}