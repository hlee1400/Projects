#ifndef Bottom_Up_ST_Maze_Solver_DFS_H
#define Bottom_Up_ST_Maze_Solver_DFS_H

#include <list>
#include <vector>

#include "Choice.h"
#include "BottomUp_SkippingMazeSolver.h"

class BottomUp_STMazeSolverDFS : public BottomUp_SkippingMazeSolver
{
private:
	SharedData& sharedData;
public:
	BottomUp_STMazeSolverDFS(Maze* maze, SharedData* sharedData)
		: BottomUp_SkippingMazeSolver(maze, sharedData), sharedData(*sharedData)
	{
		assert(pMaze);
	}

	std::vector<Direction>* Solve() override
	{
		std::vector<Choice> pChoiceStack;
		pChoiceStack.reserve(VECTOR_RESERVE_SIZE); 

		Choice ch;
		try
		{
			pChoiceStack.push_back(firstChoice(pMaze->getEnd()));

			while (!(pChoiceStack.size() == 0))
			{
				ch = pChoiceStack.back();
				if (ch.isDeadend())
				{
					pChoiceStack.pop_back();

					if (!(pChoiceStack.size() == 0))
					{
						pChoiceStack.back().pChoices.pop_frontRev();
					}

					continue;
				}
				pChoiceStack.push_back(follow(ch.at, ch.pChoices.frontRev()));

			}
			return 0;
		}
		catch (BottomUp_SolutionFoundSkip e)
		{
			AZUL_UNUSED_VAR(e); // unused

			std::vector<Choice>::iterator iter = pChoiceStack.begin();
			std::vector<Direction>* pFullPath = new std::vector<Direction>();
			pFullPath->reserve(VECTOR_RESERVE_SIZE); 

			Position curr = pMaze->getEnd();
			Direction go_to = Direction::Uninitialized;
			Direction came_from = Direction::Uninitialized;

			while (!((curr == pMaze->getStart()) || (curr == sharedData.stop.load(std::memory_order_relaxed))))
			{
				ListDirection pMoves = pMaze->GetMove(curr);

				if (Direction::Uninitialized != came_from)
				{
					pMoves.remove(came_from);
				}

				if (pMoves.size() == 1)
				{
					go_to = pMoves.frontRev();
				}
				else if (pMoves.size() > 1)
				{
					go_to = iter++->pChoices.frontRev();
				}
				else if (pMoves.size() == 0)
				{
					printf("Error in solution--leads to deadend.");
					assert(false);
				}

				pFullPath->push_back(reverseDir(go_to));
				curr = curr.move(go_to);
				came_from = reverseDir(go_to);
			}

			std::reverse(begin(*pFullPath), end(*pFullPath));

			return pFullPath;
		}
	}
};

#endif

//--- End of File ------------------------------