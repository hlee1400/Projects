#ifndef Bottom_Up_Skipping_Maze_Solver_H
#define Bottom_Up_Skipping_Maze_Solver_H

#include <exception>
#include "MazeSolver.h"
#include "Position.h"
#include "Choice.h"

#define VECTOR_RESERVE_SIZE 400000

class BottomUp_SolutionFoundSkip : public std::exception
{
public:

	BottomUp_SolutionFoundSkip(Position _pos, Direction _from)
	{
		this->pos = _pos;
		this->from = _from;
	}

	Position pos;
	Direction from;
	char pad0[4];

};

class BottomUp_SkippingMazeSolver : public MazeSolver
{
private:
	SharedData& sharedData;
public:

	BottomUp_SkippingMazeSolver(Maze* maze, SharedData* sharedData)
		: MazeSolver(maze), sharedData(*sharedData)
	{
		assert(maze);
	}

	Choice firstChoice(Position pos) 
	{
		ListDirection Moves = pMaze->GetMove(pos);

		if (Moves.size() == 1)
		{
			Direction tmp = Moves.beginRev();
			return follow(pos, tmp);
		}
		else
		{
			Choice p(pos, Direction::Uninitialized, Moves);
			return p;
		}
	}

	Choice follow(Position at, Direction dir) 
	{
		ListDirection Choices;
		Direction go_to = dir;
		Direction came_from = reverseDir(dir);
		at = at.move(go_to);

		do
		{
			if (sharedData.isCollision.load(std::memory_order_relaxed))
			{
				throw BottomUp_SolutionFoundSkip(at, reverseDir(go_to));
			}

			pMaze->isBottomCollision(at, sharedData);

			if (sharedData.isCollision.load(std::memory_order_relaxed))
			{
				throw BottomUp_SolutionFoundSkip(at, reverseDir(go_to));
			}

			if (at == pMaze->getStart())
			{
				sharedData.isCollision.store(true, std::memory_order_relaxed);
				throw BottomUp_SolutionFoundSkip(at, reverseDir(go_to));
			}

			if (at == pMaze->getEnd())
			{
				sharedData.isCollision.store(true, std::memory_order_relaxed);
				throw BottomUp_SolutionFoundSkip(at, reverseDir(go_to));
			}

			Choices = pMaze->GetMove(at);
			Choices.remove(came_from);

			if (Choices.size() == 1)
			{
				go_to = Choices.beginRev();
				at = at.move(go_to);
				came_from = reverseDir(go_to);
			}

		} while (Choices.size() == 1);

		Choice pRet(at, came_from, Choices);

		return pRet;
	}


	void markPath(std::list<Direction>* path)
	{
		try
		{
			Choice pChoice = firstChoice(pMaze->getEnd());

			Position at = pChoice.at;
			std::list<Direction>::iterator iter = path->begin();
			while (iter != path->end())
			{
				pChoice = follow(at, *iter);
				iter++;
				at = pChoice.at;
			}
		}
		catch (BottomUp_SolutionFoundSkip e)
		{
			AZUL_UNUSED_VAR(e);  // unused
		}
	}
};

#endif

//--- End of File ------------------------------