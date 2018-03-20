#ifdef _WIN64

#include "DQN.h"
#include "SimGod.h"
#include "worlds\world.h"
#include "app.h"
#include "featuremap.h"
#include "features.h"
#include "noise.h"
#include "deep-vfa-policy.h"
#include "parameters-numeric.h"
#include "../tools/CNTKWrapper/CNTKWrapper.h"

DQN::~DQN()
{
	delete m_pStateOutFeatures;
	delete[] m_pMinibatchExperienceTuples;
	delete[] m_pMinibatchChosenActionTargetValues;
	delete[] m_pMinibatchChosenActionIndex;
	CNTKWrapperLoader::UnLoad();
}

DQN::DQN(ConfigNode* pConfigNode)
{
	CNTKWrapperLoader::Load();
	m_policy = CHILD_OBJECT_FACTORY<DiscreteDeepPolicy>(pConfigNode, "Policy", "The policy");
	m_QNetwork = NEURAL_NETWORK(pConfigNode, "neural-network", "Neural Network Architecture");
	m_outputActionIndex = ACTION_VARIABLE(pConfigNode, "Output-Action", "The output action variable");
	m_experienceReplay = CHILD_OBJECT<ExperienceReplay>(pConfigNode, "experience-replay", "Experience replay", false);

	m_pStateOutFeatures = new FeatureList("state-input");

	m_numberOfStateFeatures = SimGod::getGlobalStateFeatureMap()->getTotalNumFeatures();
	if (m_numberOfStateFeatures == 0)
		Logger::logMessage(MessageType::Error, "Invalid State-Map chosen (it will return 0 features at most, but at least 1 has to be returned)");

	if (dynamic_cast<DiscreteActionFeatureMap*>(SimGod::getGlobalActionFeatureMap().get()) == nullptr)
		Logger::logMessage(MessageType::Error, "The DiscreteEpsilonGreedyDeepPolicy requires a DiscreteActionFeatureMap as the action-feature-map.");
	
	m_pGrid = ((SingleDimensionDiscreteActionVariableGrid*)(((DiscreteActionFeatureMap*)SimGod::getGlobalActionFeatureMap().get())->returnGrid()[m_outputActionIndex.get()]));

	m_numberOfActions = m_QNetwork.getNetwork()->getTotalSize();

	if (m_pGrid->getNumCenters() != m_numberOfActions)
		Logger::logMessage(MessageType::Error, "Output of the network has not the same size as the discrete action grid has centers/discrete values");

	m_stateVector = std::vector<double>(m_numberOfStateFeatures, 0.0);
	m_actionValuePredictionVector = std::vector<double>(m_numberOfActions);

	m_minibatchStateVector = std::vector<double>(m_numberOfStateFeatures * m_experienceReplay->getMaxUpdateBatchSize(), 0.0);
	m_minibatchActionValuePredictionVector = std::vector<double>(m_numberOfActions * m_experienceReplay->getMaxUpdateBatchSize(), 0.0);

	m_pMinibatchExperienceTuples = new ExperienceTuple*[m_experienceReplay->getMaxUpdateBatchSize()];
	m_pMinibatchChosenActionTargetValues = new double[m_experienceReplay->getMaxUpdateBatchSize()];
	m_pMinibatchChosenActionIndex = new int[m_experienceReplay->getMaxUpdateBatchSize()];

	m_pPredictionNetwork = nullptr;
}

INetwork* DQN::getPredictionNetwork()
{
	if (SimionApp::get()->pSimGod->getTargetFunctionUpdateFreq())
	{
		if (m_pPredictionNetwork == nullptr)
			m_pPredictionNetwork = m_QNetwork.getNetwork()->cloneNonTrainable();
		return m_pPredictionNetwork;
	}
	else
		return m_QNetwork.getNetwork();
}

double DQN::selectAction(const State * s, Action * a)
{
	SimionApp::get()->pSimGod->getGlobalStateFeatureMap()->getFeatures(s, m_pStateOutFeatures);

	//TODO: use sparse representation
	for (size_t i = 0; i < m_pStateOutFeatures->m_numFeatures; i++)
	{
		auto item = m_pStateOutFeatures->m_pFeatures[i];
		m_stateVector[item.m_index] = item.m_factor;
	}

	std::unordered_map<std::string, std::vector<double>&> inputMap = { { "state-input", m_stateVector } };

	getPredictionNetwork()->predict(inputMap, m_actionValuePredictionVector);

	size_t resultingActionIndex = m_policy->selectAction(m_actionValuePredictionVector);

	double actionValue = m_pGrid->getCenters()[resultingActionIndex];
	a->set(m_outputActionIndex.get(), actionValue);

	return 1.0;
}

double DQN::update(const State * s, const Action * a, const State * s_p, double r, double behaviorProb)
{
	double gamma = SimionApp::get()->pSimGod->getGamma();

	m_experienceReplay->addTuple(s, a, s_p, r, 1.0);

	//don't update anything if the experience replay buffer does not contain enough elements for at least one minibatch
	if (m_experienceReplay->getUpdateBatchSize() != m_experienceReplay->getMaxUpdateBatchSize())
		return 0.0;

	for (int i = 0; i < m_experienceReplay->getMaxUpdateBatchSize(); i++)
	{
		m_pMinibatchExperienceTuples[i] = m_experienceReplay->getRandomTupleFromBuffer();

		//get Q(s_p) for entire minibatch
		SimionApp::get()->pSimGod->getGlobalStateFeatureMap()->getFeatures(m_pMinibatchExperienceTuples[i]->s_p, m_pStateOutFeatures);
		for (size_t n = 0; n < m_pStateOutFeatures->m_numFeatures; n++)
			m_minibatchStateVector[m_pStateOutFeatures->m_pFeatures[n].m_index + i*m_numberOfStateFeatures] = m_pStateOutFeatures->m_pFeatures[n].m_factor;

		m_pMinibatchChosenActionIndex[i] = m_pGrid->getClosestCenter(m_pMinibatchExperienceTuples[i]->a->get(m_outputActionIndex.get()));
	}

	std::unordered_map<std::string, std::vector<double>&> inputMap = { { "state-input", m_minibatchStateVector } };
	getPredictionNetwork()->predict(inputMap, m_minibatchActionValuePredictionVector);


	//create vector of target values for the entire minibatch
	for (int i = 0; i < m_experienceReplay->getMaxUpdateBatchSize(); i++)
		m_pMinibatchChosenActionTargetValues[i] = m_pMinibatchExperienceTuples[i]->r + gamma *m_minibatchActionValuePredictionVector[
			i*m_numberOfActions + m_pMinibatchChosenActionIndex[i]];


	//get Q(s) for entire minibatch
	for (int i = 0; i < m_experienceReplay->getMaxUpdateBatchSize(); i++)
	{
		SimionApp::get()->pSimGod->getGlobalStateFeatureMap()->getFeatures(m_pMinibatchExperienceTuples[i]->s, m_pStateOutFeatures);
		for (size_t n = 0; n < m_pStateOutFeatures->m_numFeatures; n++)
			m_minibatchStateVector[m_pStateOutFeatures->m_pFeatures[n].m_index + i*m_numberOfStateFeatures] = m_pStateOutFeatures->m_pFeatures[n].m_factor;
	}
	getPredictionNetwork()->predict(inputMap, m_minibatchActionValuePredictionVector);

	for (int i = 0; i < m_experienceReplay->getMaxUpdateBatchSize(); i++)
		m_minibatchActionValuePredictionVector[m_pMinibatchChosenActionIndex[i] + i*m_numberOfActions] = m_pMinibatchChosenActionTargetValues[i];

	//update the network finally
	m_QNetwork.getNetwork()->train(inputMap, m_minibatchActionValuePredictionVector);

	if (SimionApp::get()->pSimGod->getTargetFunctionUpdateFreq())
		if (SimionApp::get()->pExperiment->getExperimentStep() % SimionApp::get()->pSimGod->getTargetFunctionUpdateFreq() == 0)
		{
			delete m_pPredictionNetwork;
			m_pPredictionNetwork = m_QNetwork.getNetwork()->cloneNonTrainable();
		}
	return 0.0; //TODO: what should we return?
}

#endif