#ifndef Top_Down_SOLVERBASE_H
#define Top_Down_SOLVERBASE_H

#include <exception>
#include <list>
#include <vector>

#include "MazeSolver.h"
#include "Position.h"
#include "Choice.h"
#include "SharedData.h"

#define VECTOR_RESERVE_SIZE 400000

class TopDown_SolutionFoundSkip : public std::exception
{
public:
    TopDown_SolutionFoundSkip(Position _pos = Position(),
        Direction f = Direction::Uninitialized)
        : pos(_pos),
        from(f),
        padding(0)
    {
    }

    Position  pos;
    Direction from;
    int       padding;
};

class TopDown_solverBase : public MazeSolver
{
private:
    SharedData& sharedData;

public:
    TopDown_solverBase(Maze* maze, SharedData* sharedData)
        : MazeSolver(maze),
        sharedData(*sharedData)
    {
        assert(maze);
    }

    Choice firstChoice(Position pos)
    {
        ListDirection moves = pMaze->getMoves(pos);

        if (moves.size() == 1)
        {
            Direction next = moves.begin();
            return follow(pos, next);
        }
        return Choice(pos, Direction::Uninitialized, moves);

    }

    Choice follow(Position at, Direction dir)
    {
        ListDirection choices;
        Direction go_to = dir;
        Direction came_from = reverseDir(dir);

        // Step once in the chosen direction
        at = at.move(go_to);

        do
        {
            // If any thread has signaled "stop", unwind
            if (sharedData.isCollision.load(std::memory_order_relaxed))
            {
                throw TopDown_SolutionFoundSkip(at, reverseDir(go_to));
            }

            // Paint this cell for the top thread and check for collision
            pMaze->isTopCollision(at, sharedData);

            if (sharedData.isCollision.load(std::memory_order_relaxed))
            {
                // We either collided or the other side already signaled stop
                throw TopDown_SolutionFoundSkip(at, reverseDir(go_to));
            }

            // If we personally reach the maze end, we also stop.
            // (We don't care about "who won", just that we should stop.)
            if (at == pMaze->getEnd())
            {
                sharedData.isCollision.store(true, std::memory_order_relaxed);
                throw TopDown_SolutionFoundSkip(at, reverseDir(go_to));
            }

            // If we ever get back to the start, that's also a terminal condition.
            if (at == pMaze->getStart())
            {
                sharedData.isCollision.store(true, std::memory_order_relaxed);
                throw TopDown_SolutionFoundSkip(at, reverseDir(go_to));
            }

            // Normal DFS corridor logic
            choices = pMaze->getMoves(at);
            choices.remove(came_from);

            if (choices.size() == 1)
            {
                go_to = choices.begin();
                at = at.move(go_to);
                came_from = reverseDir(go_to);
            }

        } while (choices.size() == 1);

        return Choice(at, came_from, choices);
    }


    void markPath(std::list<Direction>* path)
    {
        try
        {
            Choice pChoice = firstChoice(pMaze->getStart());

            Position at = pChoice.at;
            auto iter = path->begin();
            while (iter != path->end())
            {
                pChoice = follow(at, *iter);
                ++iter;
                at = pChoice.at;
            }
        }
        catch (TopDown_SolutionFoundSkip e)
        {
            AZUL_UNUSED_VAR(e);
        }
    }
};

#endif 

//--- End of File ------------------------------
