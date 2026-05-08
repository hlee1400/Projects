//------------------------------------------------------
// Copyright 2025, Ed Keenan, all rights reserved.
//------------------------------------------------------
#ifndef MT_Maze_Student_Solver_H
#define MT_Maze_Student_Solver_H

#include <list>
#include <vector>
#include <thread>

#include "Maze.h"
#include "MazeSolver.h"
#include "Direction.h"
#include "SharedData.h"
#include "TopDown_DFSsolver.h"
#include "BottomUp_STMazeSolverDFS.h"

// Feel free to change your class any way you see fit
// It needs to inherit at some point from the MazeSolver
class MTMazeStudentSolver : public MazeSolver
{
public:
    MTMazeStudentSolver(Maze* maze)
        : MazeSolver(maze)
    {
        assert(pMaze);
    }

    MTMazeStudentSolver() = delete;
    MTMazeStudentSolver(const MTMazeStudentSolver&) = delete;
    MTMazeStudentSolver& operator=(const MTMazeStudentSolver&) = delete;
    ~MTMazeStudentSolver() = default;

    std::vector<Direction>* Solve() override
    {
        SharedData sharedData;

        TopDown_STMazeSolverDFS   topSolver(pMaze, &sharedData);
        BottomUp_STMazeSolverDFS  bottomSolver(pMaze, &sharedData);

        std::vector<Direction>* pTopPath = nullptr;
        std::vector<Direction>* pBottomPath = nullptr;

        std::thread tTop([&]()
            {
                pTopPath = topSolver.Solve();
            });

        std::thread tBottom([&]()
            {
                pBottomPath = bottomSolver.Solve();
            });

        tTop.join();
        tBottom.join();

        if (pTopPath == nullptr)
        {
            pTopPath = new std::vector<Direction>();
        }

        if (pBottomPath == nullptr)
        {
            pBottomPath = new std::vector<Direction>();
        }

        const bool collided =
            sharedData.isCollision.load(std::memory_order_relaxed);

        // Single result vector for all paths / fallbacks
        auto* pResult = new std::vector<Direction>();

        if (collided)
        {
            // Collision case:
            //  - pTopPath:    start -> collision
            //  - pBottomPath: collision -> end
            pResult->reserve(pTopPath->size() + pBottomPath->size());

            pResult->insert(pResult->end(),
                pTopPath->begin(), pTopPath->end());

            pResult->insert(pResult->end(),
                pBottomPath->begin(), pBottomPath->end());
        }
        else
        {
            if (!pTopPath->empty())
            {
                *pResult = *pTopPath;
            }
            else if (!pBottomPath->empty())
            {
                *pResult = *pBottomPath;
            }
        }
        delete pTopPath;
        delete pBottomPath;

        return pResult;
    }
};


#endif

// --- End of File ----
