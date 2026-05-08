//------------------------------------------------------
// Copyright 2025, Ed Keenan, all rights reserved.
//------------------------------------------------------

// Algorithm & Maze generation - refactored and modified from material on the internet
//    Influenced by many different solvers and maze categories
//    Purpose of the maze is to create a concurrent problem set for C++ concurrency class
//    Maze solving algorithm and generators were derived from University of Maryland - Java Development
#include <atomic>
#include <list>
#include <vector>
#include <memory>

#include "Maze.h"
#include "File.h"
#include "Choice.h"

using namespace Azul;

enum class InternalBit
{
	EAST_BIT = 1,
	SOUTH_BIT = 2,
};

struct FileHeader
{
	int width;
	int height;
	int solvable;
	// int data[];  data following
};

Maze::Maze()
{
	this->width = 0;
	this->height = 0;
	this->poMazeData = nullptr;
}

Maze::~Maze()
{
	delete this->poMazeData;
}

Maze::Maze(int _width, int _height)
{
	this->width = _width;
	this->height = _height;
	unsigned int totalSize = (unsigned int)(this->width * this->height);
	this->poMazeData = new std::atomic_uint[totalSize];
	assert(poMazeData);
}

void Maze::Load(const char* const inFileName)
{
	File::Handle fh;
	File::Error  ferror;

	//----------- Open to Read  --------------------------------
	ferror = File::Open(fh, inFileName, File::Mode::READ);
	assert(ferror == File::Error::SUCCESS);

	//----------- Seek ------------------------------------------

	ferror = File::Seek(fh, File::Position::END, 0);
	assert(ferror == File::Error::SUCCESS);

	DWORD fileSize;
	ferror = File::Tell(fh, fileSize);
	assert(ferror == File::Error::SUCCESS);

	unsigned char* pBuff = new unsigned char[fileSize];
	assert(pBuff != 0);
	unsigned char* pBuff_Original = pBuff;

	ferror = File::Seek(fh, File::Position::BEGIN, 0);
	assert(ferror == File::Error::SUCCESS);

	ferror = File::Read(fh, pBuff, fileSize);
	assert(ferror == File::Error::SUCCESS);

	ferror = File::Close(fh);
	assert(ferror == File::Error::SUCCESS);

	// ----- Everything is in memory ------
	// --- Now decompress it 

	FileHeader* pHdr;

	pHdr = (FileHeader*)pBuff;

	// copy it to Maze data
	this->height = pHdr->height;
	this->width = pHdr->width;
	this->solvable = pHdr->solvable;

	pBuff += sizeof(FileHeader);

	int* pIntData = (int*)pBuff;

	// reserve the space
	unsigned int totalSize = (unsigned int)(this->width * this->height);
	this->poMazeData = new std::atomic_uint[totalSize];
	assert(this->poMazeData);
	memset(this->poMazeData, 0x0, (this->width * this->height) * sizeof(unsigned int));

	Position pos = Position(0, 0);
	while (pos.row < height)
	{
		pos = Position(pos.row, 0);
		while (pos.col < width)
		{
			int bits = *pIntData++;

			for (int bit = 0; (bit < 16 && pos.col < width); bit++)
			{
				if ((bits & 1) == 1)
				{
					setEast(pos);
				}

				if ((bits & 2) == 2)
				{
					setSouth(pos);
				}

				bits >>= 2;

				pos = pos.move(Direction::East);
			}
		}
		pos = pos.move(Direction::South);
	}

	delete pBuff_Original;
}


ListDirection Maze::getMoves(Position pos)
{
	ListDirection result;

	if (canMove(pos, Direction::South))
	{
		result.south = Direction::South;
	}
	if (canMove(pos, Direction::East))
	{
		result.east = Direction::East;
	}
	if (canMove(pos, Direction::West))
	{
		result.west = Direction::West;
	}
	if (canMove(pos, Direction::North))
	{
		result.north = Direction::North;
	}

	return result;
}

bool Maze::canMove(Position pos, Direction dir)
{
	bool status = false;

	switch (dir)
	{
	case Direction::North:
		if (pos.row == 0)
		{
			status = false;
		}
		else
		{
			status = ((getCell(pos.move(Direction::North)) & (int)InternalBit::SOUTH_BIT) == 0);
		}
		break;

	case Direction::South:
		status = ((getCell(pos) & (int)InternalBit::SOUTH_BIT) == 0);
		break;

	case Direction::East:
		status = ((getCell(pos) & (int)InternalBit::EAST_BIT) == 0);
		break;

	case Direction::West:
		if (pos.col == 0)
		{
			status = false;
		}
		else
		{
			status = ((getCell(pos.move(Direction::West)) & (int)InternalBit::EAST_BIT) == 0);
		}
		break;

	case Direction::Uninitialized:
	default:
		assert(false);
		break;
	}

	return status;

}

Position Maze::getStart()
{
	Position tmp = Position(0, this->width / 2);
	return tmp;
}


Position Maze::getEnd()
{
	Position tmp = Position(this->height - 1, this->width / 2);
	return tmp;
}

void Maze::setEast(Position pos)
{
	unsigned int newVal = getCell(pos) | (unsigned int)InternalBit::EAST_BIT;
	setCell(pos, newVal);
}

void Maze::setSouth(Position pos)
{
	unsigned int newVal = getCell(pos) | (unsigned int)InternalBit::SOUTH_BIT;
	setCell(pos, newVal);
}

unsigned int Maze::getCell(Position pos)
{
	unsigned int val = poMazeData[pos.row * this->width + pos.col];
	return val;
}

void Maze::setCell(Position pos, unsigned int val)
{
	this->poMazeData[pos.row * this->width + pos.col] = val;
}

bool Maze::checkSolution(std::vector<Direction>& soln)
{
	assert(&soln);

	bool results = true;
	Position at = getStart();

	for (auto iter = begin(soln); iter != end(soln); ++iter)
	{
		Direction dir = *iter;


		if (!canMove(at, dir))
		{
			results = false;
			break;
		}

		at = at.move(dir);
	}

	if (!(at == getEnd()))
	{
		results = false;
	}

	if (results)
	{
		Trace::out2("    checkSolution(%d elements): passed\n", (int)soln.size());
	}
	else
	{
		Trace::out2("    checkSolution(%d elements): FAILED!! <<<<-------\n", (int)soln.size());
	}

	return results;
}


void Maze::setDirectionRoute(Position at, Direction from)
{
	unsigned int val = getCell(at);
	int dir = 0;

	switch (from)
	{
	case Direction::East:  dir = 0x10000000; break;
	case Direction::West:  dir = 0x20000000; break;
	case Direction::South: dir = 0x40000000; break;
	case Direction::North: dir = 0x80000000; break;
	case Direction::Uninitialized:
		// should never happen, but explicitly handle it
		break;

	default: assert(false); break;
	}

	val |= dir;
	setCell(at, val);
}


ListDirection Maze::GetMove(Position pos)
{
	ListDirection result;

	if (canMove(pos, Direction::North))
	{
		result.north = Direction::North;
	}
	if (canMove(pos, Direction::West))
	{
		result.west = Direction::West;
	}
	if (canMove(pos, Direction::East))
	{
		result.east = Direction::East;
	}
	if (canMove(pos, Direction::South))
	{
		result.south = Direction::South;
	}

	return result;
}

void Maze::isTopCollision(Position pos, SharedData& sharedData)
{
	if ((poMazeData[(pos.row * this->width) + pos.col].fetch_or(0x8) & 0x4) == 0x4)
	{
		sharedData.stop.store(pos, std::memory_order_relaxed);
		sharedData.isCollision.store(true, std::memory_order_relaxed);
	}
}

void Maze::isBottomCollision(Position pos, SharedData& sharedData)
{
	if ((poMazeData[(pos.row * this->width) + pos.col].fetch_or(0x4) & 0x8) == 0x8)
	{
		sharedData.stop.store(pos, std::memory_order_relaxed);
		sharedData.isCollision.store(true, std::memory_order_relaxed);
	}
}



//--- End of File ------------------------------

