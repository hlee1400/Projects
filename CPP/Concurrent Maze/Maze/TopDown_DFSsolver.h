#ifndef TOPDOWN_SFDSOLVER_H
#define TOPDOWN_SFDSOLVER_H

#include <list>
#include <vector>

#include "Choice.h"
#include "TopDown_solverBase.h"

class TopDown_STMazeSolverDFS : public TopDown_solverBase
{
private:
	SharedData& sharedData;
public:
	TopDown_STMazeSolverDFS(Maze* maze, SharedData* sharedData)
		: TopDown_solverBase(maze, sharedData), sharedData(*sharedData)
	{
		assert(pMaze);
	}

	std::vector<Direction>* Solve() override
	{
		std::vector<Choice> choiceStack;
		choiceStack.reserve(VECTOR_RESERVE_SIZE);

		//Choice ch;
		try
		{
			choiceStack.push_back(firstChoice(pMaze->getStart()));

			while (!choiceStack.empty())
			{
				Choice& current = choiceStack.back();

				if (current.isDeadend())
				{
					// Backtrack: pop current choice
					choiceStack.pop_back();

					// When backtracking, advance the parent's choice list
					if (!choiceStack.empty())
					{
						choiceStack.back().pChoices.pop_front();
					}

					continue;
				}
				// Follow the first available direction from this choice
				Direction nextDir = current.pChoices.front();
				choiceStack.push_back(follow(current.at, nextDir));
			}

			// No solution found by this thread
			return nullptr;
		}

		catch (TopDown_SolutionFoundSkip e)
		{
			AZUL_UNUSED_VAR(e); // unused

            auto* pFullPath = new std::vector<Direction>();
            pFullPath->reserve(VECTOR_RESERVE_SIZE);


            Position  curr = pMaze->getStart();
            Direction lastStep = Direction::Uninitialized;
            Direction stepDir = Direction::Uninitialized;

            // Iterator walks through the recorded choices made along the DFS path
            auto stackIter = choiceStack.begin();

            // Stop when we hit the maze end or the collision stop position
            while (!((curr == pMaze->getEnd()) ||
                (curr == sharedData.stop.load(std::memory_order_relaxed))))
            {
                ListDirection moves = pMaze->getMoves(curr);

                // Do not immediately go back the way we came
                if (lastStep != Direction::Uninitialized)
                {
                    moves.remove(lastStep);
                }

                const auto numMoves = moves.size();

                if (numMoves == 1)
                {
                    // only one way to go
                    stepDir = moves.front();
                }
                else if (numMoves > 1)
                {
                    // replay the direction we took from the stack
                    stepDir = stackIter->pChoices.front();
                    ++stackIter;
                }
                else
                {
                    // Should never happen in a valid maze
                    assert(false);
                }

                pFullPath->push_back(stepDir);
                curr = curr.move(stepDir);
                lastStep = reverseDir(stepDir);
            }

            return pFullPath;
		}
	}
};

#endif

//--- End of File ------------------------------