//------------------------------------------------------
// Copyright 2025, Ed Keenan, all rights reserved.
//------------------------------------------------------

#ifndef MAZE_H
#define MAZE_H

#include <atomic>
#include <list>
#include <memory>

#include "Position.h"
#include "Direction.h"
#include "Choice.h"
#include "SharedData.h"

#define DEBUG_PRINT 0

// Maze cells are (row,col) starting at (0,0)
// Linear storage of integers 0 to (width*height - 1)
// Upper Upper corner (0,0) or index 0
// Lower Left corner (width-1, height-1) or (width*height - 1)

class Maze
{
public:
    Maze();
    Maze(const Maze&) = delete;
    Maze& operator = (const Maze&) = delete;
    ~Maze();

    Maze(int _width, int _height);

    void Load(const char* const fileName);

    ListDirection getMoves(Position pos);
    bool          canMove(Position pos, Direction dir);
    bool          checkSolution(std::vector<Direction>& soln);

    Position getStart();
    Position getEnd();

    void         setEast(Position pos);
    void         setSouth(Position pos);
    unsigned int getCell(Position pos);
    void         setCell(Position pos, unsigned int val);

    ListDirection GetMove(Position pos);
    void isTopCollision(Position pos, SharedData& sharedData);
    void isBottomCollision(Position pos, SharedData& sharedData);
    void setDirectionRoute(Position at, Direction from);

// data:
    std::atomic_uint* poMazeData;
    int height;
    int width;
    int solvable;
    int padding;                 
};

#endif 

// --- End of File ----
