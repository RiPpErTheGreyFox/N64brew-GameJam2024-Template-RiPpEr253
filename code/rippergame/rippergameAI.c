#include "./rippergameAI.h"


/*********************************
            Globals
*********************************/

AIDataStruct aiData[MAXPLAYERS];

player_data* playersRef;
int playerDataSize;
objective_data* objectivesRef;
int objectiveDataSize;
collisionobject_data* collisionObjectsRef;
int collisionObjectSize;
float speedModifierDifficulty;
int stunAbilityUseDistance;

#define DEFAULT_STUCK_TIME_LIMIT 4.0f
#define DEFAULT_DISTANCE_TO_CANCEL_CHASE 140
#define DEFAULT_DISTANCE_TO_CANCEL_ESCAPE 150
#define DEFAULT_DISTANCE_TO_SWITCH_CHASE_STATE 100
#define DEFAULT_DISTANCE_TO_SWITCH_RUNNING_STATE 100
#define SPEED_MODIFIER_EASY 5.0f
#define SPEED_MODIFIER_MEDIUM 6.0f
#define SPEED_MODIFIER_HARD 7.0f
#define STUN_ABILITY_USE_DISTANCE_EASY 20
#define STUN_ABILITY_USE_DISTANCE_MEDIUM 35
#define STUN_ABILITY_USE_DISTANCE_HARD 49


/*********************************
        State functions
*********************************/

void ai_waitingStateEnter(int aiIndex)
{
    aiData[aiIndex].currentState = StateWaiting;
    aiData[aiIndex].framesRemainingBeforeCheck = rand()%30;
}

void ai_waitingState(int aiIndex, float deltaTime, T3DVec3* newDir, float* speed)
{
    if(aiData[aiIndex].framesRemainingBeforeCheck <= 0)
    {
        ai_findNewState(aiIndex);
        return;
    }
    else
    {
        *newDir = playersRef[aiIndex].playerPos;
        *speed = 0.0f;

        aiData[aiIndex].framesRemainingBeforeCheck--;
    }
}

void ai_wanderingStateEnter(int aiIndex)
{
    aiData[aiIndex].currentState = StateWandering;
    aiData[aiIndex].framesRemainingBeforeCheck = rand()%60;

    aiData[aiIndex].destination.v[0] = (rand()%170-85) * 0.05f;
    aiData[aiIndex].destination.v[2] = (rand()%170-85) * 0.05f;
}

void ai_wanderingState(int aiIndex, float deltaTime, T3DVec3* newDir, float* speed)
{
    // super basic test AI
    if(aiData[aiIndex].framesRemainingBeforeCheck <= 0)
    {
        ai_findNewState(aiIndex);
        return;
    }
    else
    {
        if(!ai_checkForProximityBasedStateChanges(aiIndex))
        {
            *newDir = aiData[aiIndex].destination;
        
            *speed = 0.3175f;
    
            aiData[aiIndex].framesRemainingBeforeCheck--;
        }
    }
}

void ai_followingOtherPlayerStateEnter(int aiIndex)
{
    aiData[aiIndex].currentState = StateFollowingOtherPlayer;
    aiData[aiIndex].framesRemainingBeforeCheck = rand()%60;
    EntitySearchReturnData returnedStruct;
    if(playersRef[aiIndex].playerTeam == teamGuard) ai_findClosestEntityOfType(&returnedStruct, aiIndex, targetTypeThief);
    // only guards should ever be in this state, exit the state if a thief
    if(playersRef[aiIndex].playerTeam == teamThief) ai_waitingStateEnter(aiIndex);
    aiData[aiIndex].targetIndex = returnedStruct.targetIndex;
    aiData[aiIndex].targetType = returnedStruct.targetType;
}

void ai_followingOtherPlayerState(int aiIndex, float deltaTime, T3DVec3* newDir, float* speed)
{
    // check to see if we should stay in this state or not
    if(!playersRef[aiData[aiIndex].targetIndex].isActive)
    {
        // if target is not alive, then just go to waiting state
        ai_waitingStateEnter(aiIndex);
    }

    // check the timer, if it's elapsed, see if there's something closer
    if(aiData[aiIndex].framesRemainingBeforeCheck <= 0)
    {
        aiData[aiIndex].framesRemainingBeforeCheck = rand()%60;
        
        EntitySearchReturnData returnedStruct;
        ai_findClosestEntityOfType(&returnedStruct, aiIndex, targetTypeThief);

        if(!returnedStruct.isValidTarget) 
        {
            ai_waitingStateEnter(aiIndex);
            return;
        }
        else 
        {
            aiData[aiIndex].targetIndex = returnedStruct.targetIndex;
            aiData[aiIndex].targetType = returnedStruct.targetType;
        }
    }
    else
    {
        aiData[aiIndex].framesRemainingBeforeCheck--;
    }

    // body of the state starts here
    T3DVec3 tempVec = {0};
    // get position of current entity and get the diff to position of target entity
    t3d_vec3_diff(&tempVec, &playersRef[aiData[aiIndex].targetIndex].playerPos, &playersRef[aiIndex].playerPos);

    // if target has moved away far enough, exit the active chase state
    if(t3d_vec3_len(&tempVec) > DEFAULT_DISTANCE_TO_CANCEL_CHASE)
    {
        ai_waitingStateEnter(aiIndex);
        return;
    }

    // if close to other player, perform action (make this a function of difficulty?)
    if(t3d_vec3_len(&tempVec) < stunAbilityUseDistance && playersRef[aiIndex].playerTeam == teamGuard && playersRef[aiIndex].abilityTimer <= 0.0f)
    {
        player_guardAbility(deltaTime, aiIndex);
    }

    //before movement unitise then scale the vector by the full stick deflection modifier scaled with difficulty
    t3d_vec3_norm(&tempVec);
    t3d_vec3_scale(&tempVec, &tempVec, speedModifierDifficulty);
    *newDir = tempVec;

    // magic number
    *speed = 0.3175f;
}

void ai_moveToObjectiveStateEnter(int aiIndex)
{
    //TODO: Finish this function
    aiData[aiIndex].currentState = StateMoveToObjective;
    aiData[aiIndex].framesRemainingBeforeCheck = rand()%120;
    EntitySearchReturnData returnedStruct;
    ai_findClosestEntityOfType(&returnedStruct, aiIndex, targetTypeObjective);
    aiData[aiIndex].targetIndex = returnedStruct.targetIndex;
    aiData[aiIndex].targetType = returnedStruct.targetType;
}

void ai_moveToObjectiveState(int aiIndex, float deltaTime, T3DVec3* newDir, float* speed)
{
    // check to see if we should stay in this state or not
    if(!objectivesRef[aiData[aiIndex].targetIndex].isActive)
    {
        // if target is not alive, then just go to waiting state
        ai_waitingStateEnter(aiIndex);
    }

    // if the timer has elapsed, find us a new thing to do
    if(aiData[aiIndex].framesRemainingBeforeCheck <= 0)
    {
        ai_findNewState(aiIndex);
        return;
    }
    else
    {
        aiData[aiIndex].framesRemainingBeforeCheck--;
    }

    T3DVec3 tempVec = {0};
    // get position of current entity and get the diff to position of target entity
    t3d_vec3_diff(&tempVec, &objectivesRef[aiData[aiIndex].targetIndex].objectivePos, &playersRef[aiIndex].playerPos);

    // if close to the objective, just sit stil near it
    if(t3d_vec3_len(&tempVec) < 5)
    {
        *newDir = playersRef[aiIndex].playerPos;
        *speed = 0.0f;
    }
    else // else move normally
    {
        //before movement unitise then scale the vector by the full stick deflection modifier scaled with difficulty
        t3d_vec3_norm(&tempVec);
        t3d_vec3_scale(&tempVec, &tempVec, speedModifierDifficulty);
        *newDir = tempVec;

        // magic number
        *speed = 0.3175f;
    }
}

void ai_runningFromGuardStateEnter(int aiIndex)
{
    aiData[aiIndex].currentState = StateRunningFromGuard;
    aiData[aiIndex].framesRemainingBeforeCheck = rand()%60;
    EntitySearchReturnData returnedStruct;
    if(playersRef[aiIndex].playerTeam == teamThief) ai_findClosestEntityOfType(&returnedStruct, aiIndex, targetTypeGuard);
    // only thieves should ever be in this state, exit the state if a guard
    if(playersRef[aiIndex].playerTeam == teamGuard) ai_waitingStateEnter(aiIndex);
    aiData[aiIndex].targetIndex = returnedStruct.targetIndex;
    aiData[aiIndex].targetType = returnedStruct.targetType;
}

void ai_runningFromGuardState(int aiIndex, float deltaTime, T3DVec3* newDir, float* speed)
{
    // check to see if we should stay in this state or not
    if(!playersRef[aiData[aiIndex].targetIndex].isActive)
    {
        // if target is not alive, then just go to waiting state
        ai_waitingStateEnter(aiIndex);
    }

    // check the timer, if it's elapsed, see if there's something closer
    if(aiData[aiIndex].framesRemainingBeforeCheck <= 0)
    {
        aiData[aiIndex].framesRemainingBeforeCheck = rand()%60;
        
        EntitySearchReturnData returnedStruct;
        ai_findClosestEntityOfType(&returnedStruct, aiIndex, targetTypeGuard);

        if(!returnedStruct.isValidTarget) 
        {
            ai_waitingStateEnter(aiIndex);
            return;
        }
        else 
        {
            aiData[aiIndex].targetIndex = returnedStruct.targetIndex;
            aiData[aiIndex].targetType = returnedStruct.targetType;
        }
    }
    else
    {
        aiData[aiIndex].framesRemainingBeforeCheck--;
    }

    // body of the state starts here
    T3DVec3 tempVec = {0};
    // get position of current entity and get the diff to position of target entity
    t3d_vec3_diff(&tempVec, &playersRef[aiIndex].playerPos, &playersRef[aiData[aiIndex].targetIndex].playerPos);

    // if target has moved away far enough, exit the active running away state
    if(t3d_vec3_len(&tempVec) > DEFAULT_DISTANCE_TO_CANCEL_ESCAPE)
    {
        ai_waitingStateEnter(aiIndex);
        return;
    }

    // Thief ability check
    // TODO: Check for if there's a colission up ahead and use the ability

    //before movement unitise then scale the vector by the full stick deflection modifier scaled with difficulty
    t3d_vec3_norm(&tempVec);
    t3d_vec3_scale(&tempVec, &tempVec, speedModifierDifficulty);
    *newDir = tempVec;

    // magic number
    *speed = 0.3175f;
}

void ai_findClosestEntityOfType(EntitySearchReturnData* returnStruct, int aiIndex, AITargetType desiredType)
{
    // first zero out the returnStruct
    returnStruct->isValidTarget = false; returnStruct->targetIndex = 99; returnStruct->targetType = targetTypeNone; returnStruct->distanceToTarget = 0.0f;

    int tempIndex = 99; // store the current closest in a temp variable
    float tempDistance = 99999.0f; // make distance to check
    T3DVec3 tempVec = {0};

    switch(desiredType)
    {
        case targetTypeNone:
        default:
            break;
        case targetTypeObjective:
            for(int iDx = 0; iDx < objectiveDataSize; iDx++)
            {
                if(!objectivesRef[iDx].isActive) continue;

                t3d_vec3_diff(&tempVec, &playersRef[aiIndex].playerPos, &objectivesRef[iDx].objectivePos);
                if(t3d_vec3_len(&tempVec) <= tempDistance)
                {
                    returnStruct->isValidTarget = true;
                    tempIndex = iDx;
                    tempDistance = t3d_vec3_len(&tempVec);
                }
            }
            break;
        case targetTypePlayer:
        case targetTypeGuard:
        case targetTypeThief:
            for(int iDx = 0; iDx < playerDataSize; iDx++)
            {
                if(!playersRef[iDx].isActive) continue;
                if(iDx == aiIndex) continue; // make sure we don't return ourselves
                if(desiredType == targetTypeGuard && playersRef[iDx].playerTeam != teamGuard) continue;
                if(desiredType == targetTypeThief && playersRef[iDx].playerTeam != teamThief) continue;

                t3d_vec3_diff(&tempVec, &playersRef[aiIndex].playerPos, &playersRef[iDx].playerPos);
                if(t3d_vec3_len(&tempVec) <= tempDistance)
                {
                    returnStruct->isValidTarget = true;
                    tempIndex = iDx;
                    tempDistance = t3d_vec3_len(&tempVec);
                }
            }
            break;
    }
    
    returnStruct->targetIndex = tempIndex; returnStruct->distanceToTarget = tempDistance; returnStruct->targetType = desiredType;

    return;
}

// returns true if state changed, false if not
bool ai_checkForProximityBasedStateChanges(int aiIndex)
{
    EntitySearchReturnData returnedStruct;
    T3DVec3 tempVec = {0};

    if(playersRef[aiIndex].playerTeam == teamGuard)
    {
        // if there's a thief nearby, enter chasing state
        ai_findClosestEntityOfType(&returnedStruct, aiIndex, targetTypeThief);
        t3d_vec3_diff(&tempVec, &playersRef[aiIndex].playerPos, &playersRef[returnedStruct.targetIndex].playerPos);
        if(t3d_vec3_len(&tempVec) <= DEFAULT_DISTANCE_TO_SWITCH_CHASE_STATE)
        {
            ai_followingOtherPlayerStateEnter(aiIndex);
            return true;
        }

    }

    if(playersRef[aiIndex].playerTeam == teamThief)
    {
        // if there's a guard nearby, enter running state
        ai_findClosestEntityOfType(&returnedStruct, aiIndex, targetTypeGuard);
        t3d_vec3_diff(&tempVec, &playersRef[aiIndex].playerPos, &playersRef[returnedStruct.targetIndex].playerPos);
        if(t3d_vec3_len(&tempVec) <= DEFAULT_DISTANCE_TO_SWITCH_RUNNING_STATE)
        {
            ai_runningFromGuardStateEnter(aiIndex);
            return true;
        }
        return false;
    }

    return false;
}

// handles randomly selecting a new state to enter
void ai_findNewState(int aiIndex)
{
    // time for a new state, time to pick one randomly
    switch (rand()%3)
    {
        case 1:
            ai_wanderingStateEnter(aiIndex);
            break;
        case 2:
            ai_moveToObjectiveStateEnter(aiIndex);
            break;
        default:
            ai_waitingStateEnter(aiIndex);
            break;
    }
}

int  ai_getCurrentStateAsInt(int aiIndex)
{
    if(aiData[aiIndex].isAnAI == false) return 55;

    switch(aiData[aiIndex].currentState)
    {
        case StateWaiting:
            return 0;
            break;
        case StateWandering:
            return 1;
            break;
        case StateFollowingOtherPlayer:
            return 2;
            break;
        case StateMoveToObjective:
            return 3;
            break;
        case StateRunningFromGuard:
            return 4;
            break;
        case StateStuck:
            return 5;
            break;
        default:
            break;

    }
    return -1;
}

// pass in player, objective and collision pointers
void ai_init(player_data* a_players, int a_playerDataSize, objective_data* a_objectives, 
        int a_objectiveDataSize, collisionobject_data* a_collisionObjects, int a_collisionObjectSize)
{
    // initialise the AI specific globals
    playersRef = a_players;
    playerDataSize = a_playerDataSize;
    objectivesRef = a_objectives;
    objectiveDataSize = a_objectiveDataSize;
    collisionObjectsRef = a_collisionObjects;
    collisionObjectSize = a_collisionObjectSize;

    switch(core_get_aidifficulty())
    {
        case DIFF_EASY:
            stunAbilityUseDistance = STUN_ABILITY_USE_DISTANCE_EASY;
            speedModifierDifficulty = SPEED_MODIFIER_EASY;
            break;
        default:
        case DIFF_MEDIUM:
            stunAbilityUseDistance = STUN_ABILITY_USE_DISTANCE_MEDIUM;
            speedModifierDifficulty = SPEED_MODIFIER_MEDIUM;
            break;
        case DIFF_HARD:
            stunAbilityUseDistance = STUN_ABILITY_USE_DISTANCE_HARD;
            speedModifierDifficulty = SPEED_MODIFIER_HARD;
            break;
    }
    

    // initialise the AI data structs
    for(int iDx = 0; iDx < MAXPLAYERS; iDx++)
    {
        aiData[iDx].isAnAI = false;
        aiData[iDx].controlledEntityIndex = 0;
        aiData[iDx].targetIndex = 0;
        aiData[iDx].destination = (T3DVec3){{0.0f, 0.0, 0.0f}};
        aiData[iDx].stuckTimer = 0.0f;
        aiData[iDx].currentState = StateWaiting;
        ai_waitingStateEnter(iDx);
        aiData[iDx].targetType = targetTypeNone;
    }

    return;
}

int ai_assign(int a_playerNumber) // returns an index to the AI to be passed back in for ai_update
{
    aiData[a_playerNumber].controlledEntityIndex = a_playerNumber;
    aiData[a_playerNumber].isAnAI = true;
    return a_playerNumber;
}

void ai_update(int aiIndex, float deltaTime, T3DVec3* newDir, float* speed) // main update function that traverses the state machine, pass in index and deltaTime
{
    // check which state we're in and execute the correct one accordingly
    switch(aiData[aiIndex].currentState)
    {
        case StateWaiting:
            ai_waitingState(aiIndex, deltaTime, newDir, speed);
            break;
        case StateWandering:
            ai_wanderingState(aiIndex, deltaTime, newDir, speed);
            break;
        case StateFollowingOtherPlayer:
            ai_followingOtherPlayerState(aiIndex, deltaTime, newDir, speed);
            break;
        case StateMoveToObjective:
            ai_moveToObjectiveState(aiIndex, deltaTime, newDir, speed);
            break;
        case StateRunningFromGuard:
            ai_runningFromGuardState(aiIndex, deltaTime, newDir, speed);
            break;
        case StateStuck:
            //ai_stuckState(aiIndex, deltaTime, newDir, speed);
        default:
            ai_waitingStateEnter(aiIndex);
            break;

    }
    return;
}

void ai_cleanup() // ensures anything that needs to be freed, is
{
    return;
}