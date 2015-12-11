#include "stdafx.h"
#include "controller.h"
#include "parameters.h"
#include "parameter.h"
#include "globals.h"
#include "states-and-actions.h"
#include "world.h"




//LQR//////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
	/*int* m_pVariableIndices;
	double *m_pGains;
	int numVars;*/
CLQRController::CLQRController(CParameters *pParameters)
{
	CParameters* pChild= pParameters->getChild("LQR-GAINS");
	m_numVars = pChild->getNumParameters();


	m_pVariableIndices= new int[m_numVars];
	m_pGains= new double[m_numVars];

	CState* pSDesc= g_pWorld->getStateDescriptor();
	for (int i = 0; i < m_numVars; i++)
	{
		m_pVariableIndices[i] = pSDesc->getVarIndex(pChild->getParameter(i)->getName());
		m_pGains[i] = pChild->getParameter(i)->getDouble();
	}
}

CLQRController::~CLQRController()
{
	delete [] m_pVariableIndices;
	delete [] m_pGains;
}

void CLQRController::selectAction(CState *s, CAction *a)
{
	double output= 0.0; // only 1-dimension so far

	for (int i= 0; i<m_numVars; i++)
	{
		output+= s->getValue(m_pVariableIndices[i])*m_pGains[i];
	}
	// delta= -K*x
	a->setValue(0,-output);
}

//PID//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

CPIDController::CPIDController(CParameters *pParameters)
{
	m_kP= pParameters->getParameter("KP")->getDouble();
	m_kI= pParameters->getParameter("KI")->getDouble();
	m_kD= pParameters->getParameter("KD")->getDouble();

	CState *pSDesc= g_pWorld->getStateDescriptor();
	if (pSDesc)
		m_errorVariableIndex= pSDesc->getVarIndex(pParameters->getParameter("ERROR_VARIABLE")->getStringPtr());
	else
	{
		printf("ERROR: PID controller missconfigured. Invalid ERROR_VARIABLE");
		exit(-1);
	}
	m_intError= 0.0;
}



void CPIDController::selectAction(CState *s,CAction *a)
{
	if (CWorld::getT()== 0.0)
		m_intError= 0.0;

	double error= s->getValue(m_errorVariableIndex);
	double dError= error*CWorld::getDT();
	m_intError+= error*CWorld::getDT();

	//so far, it only works with 1-output controllers
	a->setValue(0,error*m_kP + m_intError*m_kI + dError*m_kD);

}

double sgn(double value)
{
	if (value<0.0) return -1.0;
	else if (value>0.0) return 1.0;

	return 0.0;
}

//VIDAL////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CWindTurbineVidalController::CWindTurbineVidalController(CParameters* pParameters)
{
	m_pA= pParameters->getParameter("A");
	m_pK_alpha = pParameters->getParameter("K_alpha");
	m_pKP = pParameters->getParameter("KP");
	m_pKI= pParameters->getParameter("KI");
	m_P_s= pParameters->getParameter("P_s")->getDouble();
}

void CWindTurbineVidalController::selectAction(CState *s,CAction *a)
{
	//f(omega_r,T_g,d_omega_r,E_p, E_int_omega_r)

	//d(Tg)/dt= (-1/omega_r)*(T_g*(a*omega_r-d_omega_r)-a*P_setpoint + K_alpha*sgn(P_a-P_setpoint))
	//d(beta)/dt= K_p*(omega_ref - omega_r) + K_i*(error_integral)
	double T_a= s->getValue("T_a");
	double omega_r= s->getValue("omega_r");
	double d_omega_r= s->getValue("d_omega_r");
	//double P_set= s->getValue("P_s");
	double error_P= s->getValue("E_p");

	double T_g= s->getValue("T_g");
	double beta= s->getValue("beta");
	
	double d_T_g;
	
	if (omega_r!=0.0) d_T_g= (-1/omega_r)*(T_g*( m_pA->getDouble() *omega_r+d_omega_r) 
		- m_pA->getDouble()*m_P_s + m_pK_alpha->getDouble()*sgn(error_P));
	else d_T_g= 0.0;

	double e_omega_r = omega_r - 4.39823; //NOMINAL WIND SPEED
	double d_beta = m_pKP->getDouble()*e_omega_r +m_pKI->getDouble()*s->getValue("E_int_omega_r");
				 /*0.5*K_p*error_omega*(1.0+sgn(error_omega))
				+ K_i*s->getValue("integrative_omega_r_error);*/

	a->setValue("d_beta",d_beta);
	a->setValue("d_T_g",d_T_g);

}

//BOUKHEZZAR CONTROLLER////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

CWindTurbineBoukhezzarController::CWindTurbineBoukhezzarController(CParameters* pParameters)
{
	m_pC_0= pParameters->getParameter("C_0");
	m_pKP = pParameters->getParameter("KP");
	m_pKI= pParameters->getParameter("KI");
	m_J_t= pParameters->getParameter("J_t")->getDouble();
	m_K_t= pParameters->getParameter("K_t")->getDouble();
}


void CWindTurbineBoukhezzarController::selectAction(CState *s,CAction *a)
{
	//d(Tg)/dt= (1/omega_r)*(C_0*error_P - (1/J_t)*(T_a*T_g - K_t*omega_r*T_g - T_g*T_g))
	//d(beta)/dt= K_p*(omega_ref - omega_r)

	double omega_r= s->getValue("omega_r");	// state->getContinuousState(DIM_omega_r);
	double C_0= m_pC_0->getDouble();					//getParameter("C0");
	double error_P= -s->getValue("E_p");		//-state->getContinuousState(DIM_P_error);
	double T_a= s->getValue("T_a");			//state->getContinuousState(DIM_T_a);

	double T_g= s->getValue("T_g");			//state->getContinuousState(DIM_T_g);
	double beta= s->getValue("beta");		//state->getContinuousState(DIM_beta);
	
	double d_T_g= (1.0/omega_r)*(C_0*error_P - (1.0/m_J_t)
		*(T_a*T_g - m_K_t*omega_r*T_g - T_g*T_g));

	double e_omega_r = omega_r - 4.39823; //NOMINAL WIND SPEED
	double d_beta = m_pKP->getDouble()*e_omega_r + m_pKI->getDouble()*s->getValue("E_int_omega_r");

	a->setValue("d_beta",d_beta); //action->setActionValue(DIM_A_beta,d_beta);
	a->setValue("d_T_g",d_T_g); //action->setActionValue(DIM_A_torque,d_T_g);

}

//JONKMAN//////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

CWindTurbineJonkmanController::CWindTurbineJonkmanController(CParameters *pParameters)
{
	//GENERATOR SPEED FILTER PARAMETERS
	m_CornerFreq= pParameters->getParameter("CornerFreq")->getDouble();

	//TORQUE CONTROLLER'S PARAMETERS
	m_VS_RtGnSp = pParameters->getParameter("VSRtGnSp")->getDouble();
	m_VS_SlPc = pParameters->getParameter("VS_SlPc")->getDouble();
	m_VS_Rgn2K = pParameters->getParameter("VS_Rgn2K")->getDouble();
	m_VS_Rgn2Sp = pParameters->getParameter("VS_Rgn2Sp")->getDouble();
	m_VS_CtInSp = pParameters->getParameter("VS_CtInSp")->getDouble();
	m_VS_RtPwr = pParameters->getParameter("VS_RtPwr")->getDouble();
	m_VS_Rgn3MP = pParameters->getParameter("VS_Rgn3MP")->getDouble();
	
	m_VS_SySp    = m_VS_RtGnSp/( 1.0 +  0.01*m_VS_SlPc );
	m_VS_Slope15 = ( m_VS_Rgn2K*m_VS_Rgn2Sp*m_VS_Rgn2Sp )/( m_VS_Rgn2Sp - m_VS_CtInSp );
	m_VS_Slope25 = ( m_VS_RtPwr/m_VS_RtGnSp           )/( m_VS_RtGnSp - m_VS_SySp   );

	if ( m_VS_Rgn2K == 0.0 )  //.TRUE. if the Region 2 torque is flat, and thus, the denominator in the ELSE condition is zero
		m_VS_TrGnSp = m_VS_SySp;
	else                          //.TRUE. if the Region 2 torque is quadratic with speed
		m_VS_TrGnSp = ( m_VS_Slope25 - sqrt( m_VS_Slope25*( m_VS_Slope25 - 4.0*m_VS_Rgn2K*m_VS_SySp ) ) )/( 2.0*m_VS_Rgn2K );

	//PITCH CONTROLLER'S PARAMETERS
	m_PC_KK = pParameters->getParameter("PC_KK")->getDouble();
	m_PC_KP = pParameters->getParameter("PC_KP")->getDouble();
	m_PC_KI = pParameters->getParameter("PC_KI")->getDouble();
	m_PC_RefSpd = pParameters->getParameter("PC_RefSpd")->getDouble();

	m_IntSpdErr= 0.0;
}

void CWindTurbineJonkmanController::selectAction(CState *s,CAction *a)
{
	//Filter the generator speed
	double Alpha;

	if (CWorld::getT()==0.0)
	{
		Alpha= 1.0;
		m_GenSpeedF= s->getValue("omega_g");
	}
	else Alpha= exp( ( CWorld::getDT() )*m_CornerFreq );
	m_GenSpeedF = ( 1.0 - Alpha )*s->getValue("omega_g") + Alpha*m_GenSpeedF;

	//TORQUE CONTROLLER
	double GenTrq;
	if ( (   m_GenSpeedF >= m_VS_RtGnSp ) || (  s->getValue("beta") >= m_VS_Rgn3MP ) )   //We are in region 3 - power is constant
		GenTrq = m_VS_RtPwr/m_GenSpeedF;
	else if ( m_GenSpeedF <= m_VS_CtInSp )                                      //We are in region 1 - torque is zero
		GenTrq = 0.0;
	else if ( m_GenSpeedF <  m_VS_Rgn2Sp )                                      //We are in region 1 1/2 - linear ramp in torque from zero to optimal
		GenTrq = m_VS_Slope15*( m_GenSpeedF - m_VS_CtInSp );
	else if ( m_GenSpeedF <  m_VS_TrGnSp )                                      //We are in region 2 - optimal torque is proportional to the square of the generator speed
		GenTrq = m_VS_Rgn2K*m_GenSpeedF*m_GenSpeedF;
	else                                                                       //We are in region 2 1/2 - simple induction generator transition region
		GenTrq = m_VS_Slope25*( m_GenSpeedF - m_VS_SySp   );

	GenTrq  = std::min( GenTrq, s->getMax("T_g")  );   //Saturate the command using the maximum torque limit

	double TrqRate;
	TrqRate = ( GenTrq - s->getValue("T_g") )/CWorld::getDT(); //Torque rate (unsaturated)
	a->setValue("d_T_g",TrqRate);

	//PITCH CONTROLLER
	double GK = 1.0/( 1.0 + s->getValue("beta")/m_PC_KK );

	//Compute the current speed error and its integral w.r.t. time; saturate the
	//  integral term using the pitch angle limits:
	double SpdErr    = m_GenSpeedF - m_PC_RefSpd;                                 //Current speed error
	m_IntSpdErr = m_IntSpdErr + SpdErr*CWorld::getDT();                           //Current integral of speed error w.r.t. time
	//Saturate the integral term using the pitch angle limits, converted to integral speed error limits
	m_IntSpdErr = std::min( std::max( m_IntSpdErr, s->getMax("beta")/( GK*m_PC_KI ) ), s->getMin("beta")/( GK*m_PC_KI ));
  
	//Compute the pitch commands associated with the proportional and integral
	//  gains:
	double PitComP   = GK*m_PC_KP*   SpdErr; //Proportional term
	double PitComI   = GK*m_PC_KI*m_IntSpdErr; //Integral term (saturated)

	//Superimpose the individual commands to get the total pitch command;
	//  saturate the overall command using the pitch angle limits:
	double PitComT   = PitComP + PitComI;                                     //Overall command (unsaturated)
	PitComT   = std::min( std::max( PitComT, s->getMin("beta") ), s->getMax("beta") );           //Saturate the overall command using the pitch angle limits

	//Saturate the overall commanded pitch using the pitch rate limit:
	//NOTE: Since the current pitch angle may be different for each blade
	//      (depending on the type of actuator implemented in the structural
	//      dynamics model), this pitch rate limit calculation and the
	//      resulting overall pitch angle command may be different for each
	//      blade.

	double d_beta= ( PitComT - s->getValue("beta") )/CWorld::getDT();
	
	a->setValue("d_beta",d_beta);
	/*
	for (int k=1; k<=NumBl; k++) //Loop through all blades
	{
		PitRate[k -1] = ( PitComT - BlPitch[k -1] )/ElapTime; //Pitch rate of blade K (unsaturated)
		PitRate[k -1] = min( max( PitRate[k -1], -PC_MaxRat ), PC_MaxRat ); //Saturate the pitch rate of blade K using its maximum absolute value
		PitCom [k -1] = BlPitch[k -1] + PitRate[k -1]*ElapTime; //Saturate the overall command of blade K using the pitch rate limit

		PitCom[k -1]  = min( max( PitCom[k -1], PC_MinPit ), PC_MaxPit ); //Saturate the overall command using the pitch angle limits         
	}  */
}