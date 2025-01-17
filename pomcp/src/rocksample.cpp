#include "rocksample.h"
#include "utils.h"

using namespace std;
using namespace UTILS;



ROCKSAMPLE::ROCKSAMPLE(int size, int rocks, const ROCKSAMPLEPARAM& rsParam, bool fix)
:   Grid(size, size),
    Size(size),
    NumRocks(rocks),
    SmartMoveProb(0.95),
    UncertaintyCount(0),
    fixed(fix)
{
    RsParam = rsParam;
    NumActions = NumRocks + 5;
    NumObservations = 3;
    RewardRange = RsParam.RewardRange;//20; <-- try to modify
    Discount = 0.95;

    if (size == 7 && rocks == 8)
        Init_7_8();
    else if (size == 11 && rocks == 11)
        Init_11_11();
    else
        InitGeneral();

}

bool ROCKSAMPLE_STATE::isEqual(const STATE &a) const{

    const auto &A = safe_cast<const ROCKSAMPLE_STATE&>(a);

    if(Rocks.size() != A.Rocks.size())
        return false;
    if(view != A.view)
        return false;

    return true;

}

void ROCKSAMPLE::InitGeneral()
{
    //HalfEfficiencyDistance = 20;
    StartPos = COORD(0, Size / 2);
    RandomSeed(0);
    Grid.SetAllValues(-1);
    for (int i = 0; i < NumRocks; ++i)
    {
        COORD pos;
        do
        {
            pos = COORD(Random(Size), Random(Size));
        }
        while (Grid(pos) >= 0);
        Grid(pos) = i;
        RockPos.push_back(pos);
    }
}

void ROCKSAMPLE::Init_7_8()
{
    // Equivalent to RockSample_7_8.pomdpx
    cout << "Using special layout for rocksample(7, 8)" << endl;

    COORD rocks[] =
    {
        COORD(2, 0),
        COORD(0, 1),
        COORD(3, 1),
        COORD(6, 3),
        COORD(2, 4),
        COORD(3, 4),
        COORD(5, 5),
        COORD(1, 6)
    };

    //HalfEfficiencyDistance = 20;
    StartPos = COORD(0, 3);
    Grid.SetAllValues(-1);
    for (int i = 0; i < NumRocks; ++i)
    {
        Grid(rocks[i]) = i;
        RockPos.push_back(rocks[i]);
    }
}

void ROCKSAMPLE::Init_11_11()
{
    // Equivalent to RockSample_11_11.pomdp(x)
    cout << "Using special layout for rocksample(11, 11)" << endl;

    COORD rocks[] =
    {
        COORD(0, 3),
        COORD(0, 7),
        COORD(1, 8),
        COORD(2, 4),
        COORD(3, 3),
        COORD(3, 8),
        COORD(4, 3),
        COORD(5, 8),
        COORD(6, 1),
        COORD(9, 3),
        COORD(9, 9)
    };

    //HalfEfficiencyDistance = 20;
    StartPos = COORD(0, 5);
    Grid.SetAllValues(-1);
    for (int i = 0; i < NumRocks; ++i)
    {
        Grid(rocks[i]) = i;
        RockPos.push_back(rocks[i]);
    }
}


STATE* ROCKSAMPLE::Copy(const STATE& state) const
{
    const ROCKSAMPLE_STATE& rockstate = safe_cast<const ROCKSAMPLE_STATE&>(state);
    ROCKSAMPLE_STATE* newstate = MemoryPool.Allocate();
    *newstate = rockstate;
    return newstate;
}

void ROCKSAMPLE::Validate(const STATE& state) const
{
    const ROCKSAMPLE_STATE& rockstate = safe_cast<const ROCKSAMPLE_STATE&>(state);
    assert(Grid.Inside(rockstate.AgentPos));
}

STATE* ROCKSAMPLE::CreateStartState() const
{
    ROCKSAMPLE_STATE* rockstate = MemoryPool.Allocate();
    rockstate->AgentPos = StartPos;
    rockstate->Rocks.clear();
    for (int i = 0; i < NumRocks; i++)
    {
        ROCKSAMPLE_STATE::ENTRY entry;
        entry.Collected = false;

        if(fixed){
            if(RsParam.valuableRocks == "" || RsParam.valuableRocks.length() != NumRocks)
                entry.Valuable = Bernoulli(0.5);
            else{
                char value = RsParam.valuableRocks[NumRocks-i -1];
                if(value == '0')
                    entry.Valuable = false;
                else if(value == '1')
                    entry.Valuable = true;
                else
                    entry.Valuable = Bernoulli(0.5);
            }
        }else{
            entry.Valuable = Bernoulli(0.5);
        }
    

        entry.Count = 0;
        entry.Measured = 0;
        entry.ProbValuable = 0.5;
        entry.LikelihoodValuable = 1.0;
        entry.LikelihoodWorthless = 1.0;
        rockstate->Rocks.push_back(entry);
    }


    rockstate->view = "";
    for(int i=0; i < NumRocks; i++){
        if(rockstate->Rocks[NumRocks-i -1].Valuable)
            rockstate->view += '1';
        else
            rockstate->view+= '0';
    }

    //std::cout << "view dello stato: " << rockstate->view << std::endl;

    rockstate->Target = SelectTarget(*rockstate);
    return rockstate;
}

void ROCKSAMPLE::FreeState(STATE* state) const
{
    ROCKSAMPLE_STATE* rockstate = safe_cast<ROCKSAMPLE_STATE*>(state);
    MemoryPool.Free(rockstate);
}

bool ROCKSAMPLE::Step(STATE& state, int action,
    int& observation, double& reward) const
{
    ROCKSAMPLE_STATE& rockstate = safe_cast<ROCKSAMPLE_STATE&>(state);
    reward = 0;
    observation = E_NONE;


    if (action < E_SAMPLE) // move
    {
        switch (action)
        {
            case COORD::E_EAST:
                if (rockstate.AgentPos.X + 1 < Size){
                    rockstate.AgentPos.X++;
                    break;
                }
                else
                {
                    reward = RsParam.rEast;
                    return true;
                }

            case COORD::E_NORTH:
                if (rockstate.AgentPos.Y + 1 < Size){
                    rockstate.AgentPos.Y++;
                    break;
                }
                else{
                    reward = RsParam.rNorth;
                    return true;
                }
            case COORD::E_SOUTH:
                if (rockstate.AgentPos.Y - 1 >= 0){
                    rockstate.AgentPos.Y--;
                    break;
                }
                else{
                    reward = RsParam.rSouth;
                    return true;
                }
            case COORD::E_WEST:
                if (rockstate.AgentPos.X - 1 >= 0){
                    rockstate.AgentPos.X--;
                    break;
                }
                else{
                    reward = RsParam.rWest;
                    return true; 
                }
        }
    }

    if (action == E_SAMPLE) // sample
    {
        int rock = Grid(rockstate.AgentPos);
        if (rock >= 0 && !rockstate.Rocks[rock].Collected)
        {
            rockstate.Rocks[rock].Collected = true;
            if (rockstate.Rocks[rock].Valuable){
                reward = RsParam.rValuable;
            }
            else{
                reward = RsParam.rNotValuable;
            }
        }
        else
        {
            reward = RsParam.rAlreadySampled;
        }
    }

    if (action > E_SAMPLE) // check
    {
        int rock = action - E_SAMPLE - 1;
        assert(rock < NumRocks);
        observation = GetObservation(rockstate, rock);
        rockstate.Rocks[rock].Measured++;

        double distance = COORD::EuclideanDistance(rockstate.AgentPos, RockPos[rock]);
    	double efficiency = (1 + pow(2, -distance / RsParam.HalfEfficiencyDistance)) * 0.5;

        if (observation == E_GOOD)
        {
            rockstate.Rocks[rock].Count++;
            rockstate.Rocks[rock].LikelihoodValuable *= efficiency;
            rockstate.Rocks[rock].LikelihoodWorthless *= 1.0 - efficiency;

        }
        else
        {
            rockstate.Rocks[rock].Count--;
            rockstate.Rocks[rock].LikelihoodWorthless *= efficiency;
            rockstate.Rocks[rock].LikelihoodValuable *= 1.0 - efficiency;
		}
		double denom = (0.5 * rockstate.Rocks[rock].LikelihoodValuable) +
			(0.5 * rockstate.Rocks[rock].LikelihoodWorthless);
		rockstate.Rocks[rock].ProbValuable = (0.5 * rockstate.Rocks[rock].LikelihoodValuable) / denom;
    }

    if (rockstate.Target < 0 || rockstate.AgentPos == RockPos[rockstate.Target])
        rockstate.Target = SelectTarget(rockstate);

    //assert(reward != -100);
    return false;
}


int ROCKSAMPLE::reward(const STATE& state, int action) const{

    const ROCKSAMPLE_STATE& rockstate = safe_cast<const ROCKSAMPLE_STATE&>(state);
    if (action < E_SAMPLE){ // move
        switch (action){
            case COORD::E_EAST:
                if (!(rockstate.AgentPos.X + 1 < Size))
                    return RsParam.rEast;
            case COORD::E_NORTH:
                if (!(rockstate.AgentPos.Y + 1 < Size))
                    return RsParam.rNorth;
            case COORD::E_SOUTH:
                if (!(rockstate.AgentPos.Y - 1 >= 0))
                    return RsParam.rSouth;
            case COORD::E_WEST:
                if (!(rockstate.AgentPos.X - 1 >= 0))
                    return RsParam.rWest;
        }
    }

    if (action == E_SAMPLE){ // sample
        int rock = Grid(rockstate.AgentPos);
        if (rock >= 0 && !rockstate.Rocks[rock].Collected){
            if (rockstate.Rocks[rock].Valuable)
                return RsParam.rValuable;
            else
                return RsParam.rNotValuable;
        }
        else
            return RsParam.rAlreadySampled;
    }


    //check == no reward
    return 0;
}


double ROCKSAMPLE::Rho_reward(const BAG& belief, int action) const{
    double r = 0;
    for (auto& element : belief.getContainer()){
        //std::cout << "reward nel rho reward: " << reward(*element.first,action) << std::endl;
        r += reward(*element.first,action) * belief.GetNormalizedWeight(element.first);
    }

    return r;
}


double ROCKSAMPLE::ProbObs(int observation, const STATE& startingState, int action, const STATE& finalState) const{

    if (action <= E_SAMPLE){ //se l'azione è di movimento o di sample (quindi azione non di check)
        return observation == E_NONE ? 1.0: 0.0;
    } 

    //azione di check
    const ROCKSAMPLE_STATE startRSstate = static_cast<const ROCKSAMPLE_STATE&>(startingState);
    const ROCKSAMPLE_STATE finalRSstate = static_cast<const ROCKSAMPLE_STATE&>(finalState);
    
    int rock = action - E_SAMPLE - 1;
    assert(rock < NumRocks);

    double distance = COORD::EuclideanDistance(startRSstate.AgentPos, RockPos[rock]);
    double efficiency = (1 + pow(2, -distance / RsParam.HalfEfficiencyDistance)) * 0.5;

    if(startRSstate.Rocks[rock].Valuable)
        return observation == E_GOOD ? efficiency : 1- efficiency;
    else
        return observation == E_BAD? efficiency : 1 - efficiency;
}




bool ROCKSAMPLE::LocalMove(STATE& state, const HISTORY& history,
    int stepObs, const STATUS& status) const
{
    ROCKSAMPLE_STATE& rockstate = safe_cast<ROCKSAMPLE_STATE&>(state);
    int rock = Random(NumRocks);
    rockstate.Rocks[rock].Valuable = !rockstate.Rocks[rock].Valuable;

    if (history.Back().Action > E_SAMPLE) // check rock
    {
        rock = history.Back().Action - E_SAMPLE - 1;
        int realObs = history.Back().Observation;

        // Condition new state on real observation
        int newObs = GetObservation(rockstate, rock);
        if (newObs != realObs)
            return false;

        // Update counts to be consistent with real observation
        if (realObs == E_GOOD && stepObs == E_BAD)
            rockstate.Rocks[rock].Count += 2;
        if (realObs == E_BAD && stepObs == E_GOOD)
            rockstate.Rocks[rock].Count -= 2;
    }
    return true;
}

void ROCKSAMPLE::GenerateLegal(const STATE& state, const HISTORY& history,
    vector<int>& legal, const STATUS& status) const
{

    const ROCKSAMPLE_STATE& rockstate =
        safe_cast<const ROCKSAMPLE_STATE&>(state);


    if(RsParam.rNorth>0)
        legal.push_back(COORD::E_NORTH);
    else
        if (rockstate.AgentPos.Y + 1 < Size)
            legal.push_back(COORD::E_NORTH);

    if(RsParam.rEast>0)
        legal.push_back(COORD::E_EAST);
    else
        if (rockstate.AgentPos.X + 1 < Size)
            legal.push_back(COORD::E_EAST);

    if(RsParam.rSouth>0)
        legal.push_back(COORD::E_SOUTH);
    else
        if (rockstate.AgentPos.Y - 1 >= 0)
            legal.push_back(COORD::E_SOUTH);

    if(RsParam.rWest>0)
        legal.push_back(COORD::E_WEST);
    else
        if (rockstate.AgentPos.X - 1 >= 0)
            legal.push_back(COORD::E_WEST);



    int rock = Grid(rockstate.AgentPos);
    if (rock >= 0 && !rockstate.Rocks[rock].Collected)
        legal.push_back(E_SAMPLE);

    for (rock = 0; rock < NumRocks; ++rock)
        if (!rockstate.Rocks[rock].Collected)
            legal.push_back(rock + 1 + E_SAMPLE);
}

void ROCKSAMPLE::GeneratePreferred(const STATE& state, const HISTORY& history,
    vector<int>& actions, const STATUS& status) const
{

/*	static const bool UseBlindPolicy = false;

	if (UseBlindPolicy)
	{
		actions.push_back(COORD::E_EAST);
		return;
	}
*/
	const ROCKSAMPLE_STATE& rockstate =
	        safe_cast<const ROCKSAMPLE_STATE&>(state);

	// Sample rocks with more +ve than -ve observations
	int rock = Grid(rockstate.AgentPos);
	if (rock >= 0 && !rockstate.Rocks[rock].Collected)
	{
		int total = 0;
		for (int t = 0; t < history.Size(); ++t)
		{
			if (history[t].Action == rock + 1 + E_SAMPLE)
			{
				if (history[t].Observation == E_GOOD)
					total++;
				if (history[t].Observation == E_BAD)
					total--;
			}
		}
		if (total > 0)
		{
			actions.push_back(E_SAMPLE);
			return;
		}

	}

	// processes the rocks
	bool all_bad = true;
	bool north_interesting = false;
	bool south_interesting = false;
	bool west_interesting  = false;
	bool east_interesting  = false;

	for (int rock = 0; rock < NumRocks; ++rock)
	{
		const ROCKSAMPLE_STATE::ENTRY& entry = rockstate.Rocks[rock];
		if (!entry.Collected)
		{
			int total = 0;
			for (int t = 0; t < history.Size(); ++t)
			{
				if (history[t].Action == rock + 1 + E_SAMPLE)
				{
					if (history[t].Observation == E_GOOD)
						total++;
					if (history[t].Observation == E_BAD)
						total--;
				}
			}

			if (total >= 0)
			{
				all_bad = false;

				if (RockPos[rock].Y > rockstate.AgentPos.Y)
					north_interesting = true;
				if (RockPos[rock].Y < rockstate.AgentPos.Y)
					south_interesting = true;
				if (RockPos[rock].X < rockstate.AgentPos.X)
					west_interesting = true;
				if (RockPos[rock].X > rockstate.AgentPos.X)
					east_interesting = true;
			}
		}
	}

	// if all remaining rocks seem bad, then head east
    // check in all direction if the reward is positive, else head east.
	if (all_bad)
	{
        if(RsParam.rEast>0)
            actions.push_back(COORD::E_EAST);
        if(RsParam.rNorth>0)
            actions.push_back(COORD::E_NORTH);
        if(RsParam.rSouth>0)
            actions.push_back(COORD::E_SOUTH);
        if(RsParam.rWest>0)
            actions.push_back(COORD::E_WEST);
        if(RsParam.rWest<=0 && RsParam.rEast<= 0 && RsParam.rNorth<=0 && RsParam.rSouth<=0)
            actions.push_back(COORD::E_EAST);
		//return;
    }

	// generate a random legal move, with the exceptions that:
	//   a) there is no point measuring a rock that is already collected
	//   b) there is no point measuring a rock too often
	//   c) there is no point measuring a rock which is clearly bad or good
	//   d) we never sample a rock (since we need to be sure)
	//   e) we never move in a direction that doesn't take us closer to
	//      either the edge of the map or an interesting rock


    if(RsParam.rNorth>0)
        if (north_interesting)
            actions.push_back(COORD::E_NORTH);
    else
        if (rockstate.AgentPos.Y + 1 < Size && north_interesting)
			actions.push_back(COORD::E_NORTH);

    if(RsParam.rEast>0)
        if (east_interesting)
            actions.push_back(COORD::E_EAST);
    else
        if (rockstate.AgentPos.X+1 < Size && east_interesting)
		  actions.push_back(COORD::E_EAST);

    if(RsParam.rSouth>0)
        if (south_interesting)
            actions.push_back(COORD::E_SOUTH);
    else
        if (rockstate.AgentPos.Y - 1 >= 0 && south_interesting)
		  actions.push_back(COORD::E_SOUTH);

    if(RsParam.rWest>0)
        if (west_interesting)
            actions.push_back(COORD::E_WEST);
    else
        if (rockstate.AgentPos.X - 1 >= 0 && west_interesting)
		  actions.push_back(COORD::E_WEST);




	for (rock = 0; rock < NumRocks; ++rock)
	{
		if (!rockstate.Rocks[rock].Collected    &&
			rockstate.Rocks[rock].ProbValuable != 0.0 &&
			rockstate.Rocks[rock].ProbValuable != 1.0 &&
			rockstate.Rocks[rock].Measured < 2  &&
			std::abs(rockstate.Rocks[rock].Count) < 2)
		{
			actions.push_back(rock + 1 + E_SAMPLE);
		}
	}

  //  for(auto a : actions){
  //      std::cout << a << " ";
  //  }
  //  std::cout << std::endl;

}

int ROCKSAMPLE::GetObservation(const ROCKSAMPLE_STATE& rockstate, int rock) const
{
    double distance = COORD::EuclideanDistance(rockstate.AgentPos, RockPos[rock]);
    double efficiency = (1 + pow(2, -distance / RsParam.HalfEfficiencyDistance)) * 0.5;

    if (Bernoulli(efficiency))
        return rockstate.Rocks[rock].Valuable ? E_GOOD : E_BAD;
    else
        return rockstate.Rocks[rock].Valuable ? E_BAD : E_GOOD;
}

int ROCKSAMPLE::SelectTarget(const ROCKSAMPLE_STATE& rockstate) const
{
    int bestDist = Size * 2;
    int bestRock = -1;
    for (int rock = 0; rock < NumRocks; ++rock)
    {
        if (!rockstate.Rocks[rock].Collected
            && rockstate.Rocks[rock].Count >= UncertaintyCount)
        {
            int dist = COORD::ManhattanDistance(rockstate.AgentPos, RockPos[rock]);
            if (dist < bestDist)
                bestDist = dist;
        }
    }
    return bestRock;
}

void ROCKSAMPLE::DisplayBeliefs(const BAG& beliefState,
    std::ostream& ostr) const
{
     if(beliefState.Empty())
        return;

    ostr << "bag: \n";
    for (auto& element : beliefState.getContainer()){
        ostr << "[ ";
        DisplayState(*element.first,ostr);
        ostr << " PESO: " << element.second << " ]\n";
    }
    
    ostr << std::endl;
}

void ROCKSAMPLE::DisplayState(const STATE& state, std::ostream& ostr) const
{
    const ROCKSAMPLE_STATE& rockstate = safe_cast<const ROCKSAMPLE_STATE&>(state);
    ostr << endl;
    for (int x = 0; x < Size + 2; x++)
        ostr << "# ";
    ostr << endl;
    for (int y = Size - 1; y >= 0; y--)
    {
        ostr << "# ";
        for (int x = 0; x < Size; x++)
        {
            COORD pos(x, y);
            int rock = Grid(pos);
            const ROCKSAMPLE_STATE::ENTRY& entry = rockstate.Rocks[rock];
            if (rockstate.AgentPos == COORD(x, y))
                ostr << "* ";
            else if (rock >= 0 && !entry.Collected)
                ostr << rock << (entry.Valuable ? "$" : "X");
            else
                ostr << ". ";
        }
        ostr << "#" << endl;
    }
    for (int x = 0; x < Size + 2; x++)
        ostr << "# ";
    ostr << endl;
  //  ostr << rockstate.view;
}

void ROCKSAMPLE::DisplayObservation(const STATE& state, int observation, std::ostream& ostr) const
{
    switch (observation)
    {
    case E_NONE:
        break;
    case E_GOOD:
        ostr << "Observed good" << endl;
        break;
    case E_BAD:
        ostr << "Observed bad" << endl;
        break;
    }
}

void ROCKSAMPLE::DisplayAction(int action, std::ostream& ostr) const
{
    if (action < E_SAMPLE)
        ostr << COORD::CompassString[action] << endl;
    if (action == E_SAMPLE)
        ostr << "Sample" << endl;
    if (action > E_SAMPLE)
        ostr << "Check " << action - E_SAMPLE << endl;
}




//xes


//va bene
void ROCKSAMPLE::log_problem_info() const {
    XES::logger().add_attributes({
            {"problem", "rocksample"},
            {"RewardRange", RewardRange},
            {"HalfEfficiencyDistance", RsParam.HalfEfficiencyDistance},
            {"Size", Size},
            {"NumRocks", NumRocks}
        });

    XES::logger().start_list("rocks");
    for (int i = 0; i < RockPos.size(); i++) {
        XES::logger().start_list("rock");
        XES::logger().add_attributes({
                {"number", i},
                {"coord x", RockPos[i].X},
                {"coord y", RockPos[i].Y},
                });
        XES::logger().end_list();
    }
    XES::logger().end_list(); // end rocks
}



//va bene
void ROCKSAMPLE::log_beliefs(const BAG& beliefState, int action , int observation ) const {


    if(action == -1){
        //print agent's position
        XES::logger().start_list("Root");
        
        const STATE& s = safe_cast<const STATE&>(*beliefState.GetFirstSample());
        auto* rstate = safe_cast<const ROCKSAMPLE_STATE*>(&s);

        XES::logger().add_attribute({"coord x", rstate->AgentPos.X });
        XES::logger().add_attribute({"coord y", rstate->AgentPos.Y });


        //print belief



        for (auto& element : beliefState.getContainer()){
            const ROCKSAMPLE_STATE* rs = safe_cast<ROCKSAMPLE_STATE* >(element.first);
            XES::logger().add_attribute({rs->view, beliefState.GetNormalizedWeight(element.first)});
        }
        XES::logger().add_attribute({"Num of particle", (int)beliefState.GetTotalWeight()});

        XES::logger().end_list();

    } else{

         XES::logger().start_list("next belief with action: " + std::to_string(action) + " and observation: " +std::to_string(observation));

        for (auto& element : beliefState.getContainer()){
            const ROCKSAMPLE_STATE* rs = safe_cast<ROCKSAMPLE_STATE* >(element.first);
            XES::logger().add_attribute({rs->view, beliefState.GetNormalizedWeight(element.first)});
        }
        XES::logger().add_attribute({"Num of particle", (int)beliefState.GetTotalWeight()});

        XES::logger().end_list();


    }
  
    
}


//va bene
void ROCKSAMPLE::log_state(const STATE& state) const {
    const ROCKSAMPLE_STATE& rs = safe_cast<const ROCKSAMPLE_STATE&>(state);
    XES::logger().start_list("state");
    XES::logger().add_attribute({"coord x", rs.AgentPos.X });
    XES::logger().add_attribute({"coord y", rs.AgentPos.Y });
    XES::logger().start_list("rocks");
    int i = 0;
    for (const auto &r : rs.Rocks) {
        XES::logger().start_list("rock");
        XES::logger().add_attributes({
                {"number", i},
                {"valuable", r.Valuable},
                {"collected", r.Collected}
                });
        XES::logger().end_list();
        ++i;
    }
    XES::logger().end_list(); // end rocks
    XES::logger().end_list(); // end state
}


//va bene
void ROCKSAMPLE::log_action(int action) const {
    switch (action) {
        case COORD::E_EAST:
            XES::logger().add_attribute({"action", "east"});
            return;

        case COORD::E_NORTH:
            XES::logger().add_attribute({"action", "north"});
            return;

        case COORD::E_SOUTH:
            XES::logger().add_attribute({"action", "south"});
            return;

        case COORD::E_WEST:
            XES::logger().add_attribute({"action", "west"});
            return;
        case E_SAMPLE:
            XES::logger().add_attribute({"action", "sample"});
            return;
        default:
            int rock = action - E_SAMPLE - 1;
            XES::logger().add_attribute({"action", "check rock " + std::to_string(rock)});
            return;
    }
}


//va bene
void ROCKSAMPLE::log_observation(const STATE&, int observation) const {
    switch (observation) {
        case E_NONE:
            XES::logger().add_attribute({"observation", "none"});
            return;
        case E_GOOD:
            XES::logger().add_attribute({"observation", "good"});
            return;
        case E_BAD:
            XES::logger().add_attribute({"observation", "bad"});
            return;
    }
}

//va bene
void ROCKSAMPLE::log_reward(double reward) const{
    XES::logger().add_attribute({"reward", reward});
}

