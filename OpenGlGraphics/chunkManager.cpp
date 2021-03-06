#include "chunkManager.h"
#include "globals.h"

int bufferWidth = 10;
#define SEALEVEL 57
#define DESERTSTEP 0.5

#define INFLUENCE_CONTINENTAL 100
#define INFLUENCE_MAJ 120
#define INFLUENCE_MED 70
#define INFLUENCE_MIN 1.5

int m_mod(const int b, const int a)
{
	return (b % a + a) % a;
}
int m_xyzToIndex(const int x, const int y, const int z)
{
	return x + CHUNKWIDTH * z + CHUNKWIDTH * CHUNKWIDTH * y;
}

int m_xzToChunkIndex(int x, int z)
{
	return m_mod(x, bufferWidth) + bufferWidth * m_mod(z, bufferWidth);
}

double m_clamp(const double val, const double max, const double min)
{
	return std::fmin(max, std::fmax(val, min));
}

void GetCoordsOnMap(int chunkX, int chunkZ, int blockX, int blockZ, double& xCoord, double& zCoord)
{
	xCoord = (double)(chunkX * CHUNKWIDTH + blockX) / 1000000;
	zCoord = (double)(chunkZ * CHUNKWIDTH + blockZ) / 1000000;
}

char& ChunkManager::m_xyzToBlock(float globalX, float globalY, float globalZ)
{
	globalX = floor(globalX);
	globalZ = floor(globalZ);

	int chunkX = floor(globalX / CHUNKWIDTH);
	int chunkZ = floor(globalZ / CHUNKWIDTH);

	int blockX = m_mod(globalX, CHUNKWIDTH);
	int blockZ = m_mod(globalZ, CHUNKWIDTH);
	int blockY = globalY;

	Chunk* chunk = m_chunks[m_xzToChunkIndex(chunkX, chunkZ)];

	return chunk->blockIDs[m_xyzToIndex(blockX, blockY, blockZ)];
}

ChunkManager::ChunkManager(Blocks* blocks, Display* display)
{
	bufferWidth = 2 * drawDistance;
	std::cout << "Async Loading " << asyncLoading << std::endl;
	m_blocks = blocks;
	m_display = display;
	m_structures = Structures();

	m_chunks.reserve(bufferWidth * bufferWidth);
	for (int i = 0; i < bufferWidth * bufferWidth; i++)
		m_chunks.push_back(nullptr);

	m_mapHeightContinental.SetNoiseType(FastNoise::Perlin);
	m_mapHeightContinental.SetFrequency(850);
	m_mapHeightContinental.SetInterp(FastNoise::Hermite);
	m_mapHeightContinental.SetSeed(m_seed);

	m_mapHeightMaj.SetNoiseType(FastNoise::Perlin);
	m_mapHeightMaj.SetFrequency(6000);
	m_mapHeightMaj.SetInterp(FastNoise::Hermite);
	m_mapHeightMaj.SetSeed(m_seed * 1.2 + 1269);

	m_mapHeightMed.SetNoiseType(FastNoise::PerlinFractal);
	m_mapHeightMed.SetFrequency(8000);
	m_mapHeightMed.SetInterp(FastNoise::Hermite);
	m_mapHeightMed.SetSeed(m_seed * 0.9 - 1597);

	m_mapHeightMin.SetNoiseType(FastNoise::Perlin);
	m_mapHeightMin.SetFrequency(110000);
	m_mapHeightMin.SetInterp(FastNoise::Hermite);
	m_mapHeightMin.SetSeed((m_seed % 10912) * 8.4);

	m_mapTemp.SetNoiseType(FastNoise::Perlin);
	m_mapTemp.SetFrequency(800);
	m_mapTemp.SetInterp(FastNoise::Hermite);
	m_mapTemp.SetSeed(m_seed * 3.4 - m_seed % 1027);

	m_mapVariety.SetNoiseType(FastNoise::Perlin);
	m_mapVariety.SetFrequency(3000);
	m_mapVariety.SetInterp(FastNoise::Hermite);
	m_mapVariety.SetSeed(m_seed * 1.4 - m_seed % 98765);

	m_mapFoliageDensity.SetNoiseType(FastNoise::Perlin);
	m_mapFoliageDensity.SetFrequency(3500);
	m_mapFoliageDensity.SetInterp(FastNoise::Hermite);
	m_mapFoliageDensity.SetSeed(m_seed + 1897);

	m_mapTreeDensity.SetNoiseType(FastNoise::Perlin);
	m_mapTreeDensity.SetFrequency(3500);
	m_mapTreeDensity.SetInterp(FastNoise::Hermite);
	m_mapTreeDensity.SetSeed(pow(m_seed, 2.1) - 185);

	m_mapSandArea.SetNoiseType(FastNoise::PerlinFractal);
	m_mapSandArea.SetFrequency(3000);
	m_mapSandArea.SetInterp(FastNoise::Hermite);
	m_mapSandArea.SetSeed(10983102 % max(1, m_seed) + m_seed * 0.1);
	m_mapSandArea.SetFractalOctaves(5);

	m_mapBeachHeight.SetNoiseType(FastNoise::Perlin);
	m_mapBeachHeight.SetFrequency(18000);
	m_mapBeachHeight.SetInterp(FastNoise::Hermite);
	m_mapBeachHeight.SetSeed(m_seed * 0.123 + 687431);

	srand(m_seed * 124 - m_seed % 987);
}

ChunkManager::~ChunkManager()
{
}

void ChunkManager::Draw(float x, float z)
{
	//TEST
	//Vertex v[4] = {Vertex(glm::vec3(0, 0, 0)}
	//std::cout << Mesh::RayFaceIntersectionTEST(camPosition, camForward, m_c);

	//
	//m_display->Clear(0.7f + (m_mapTemp.GetNoise(x / 1000000, z / 1000000)) * 0.5, 0.9f + (m_mapTemp.GetNoise(x / 1000000, z / 1000000)) * 0.08, 0.98f, 1.0f);
	m_display->Clear(0.7f, 0.9f, 0.98f, 1.0f);

	int xPos = floor((double)x / CHUNKWIDTH);
	int zPos = floor((double)z / CHUNKWIDTH);

	if (m_bufferNeedsToBeReAssigned)
	{
		m_display->ReassignBuffer();
		m_bufferNeedsToBeReAssigned = false;
	}

	if ((xPos != m_old_xPos || zPos != m_old_zPos) && !m_loadingThreadRunning)
	{
		m_display->ClearBuffer();

		if (m_loadingThread.joinable())
			m_loadingThread.join();

		if (asyncLoading)
			m_loadingThread = std::thread(&ChunkManager::UpdateBuffer, this, xPos, zPos);
		else
			UpdateBuffer(xPos, zPos);
		// ASYNC_LOADING

		m_old_xPos = xPos;
		m_old_zPos = zPos;
	}
}

void ChunkManager::UpdateBuffer(int chunkX, int chunkZ)
{
	m_loadingThreadRunning = true;
	for (int x = chunkX - drawDistance; x <= chunkX + drawDistance; x++)
		for (int z = chunkZ - drawDistance; z <= chunkZ + drawDistance; z++)
			if (m_chunks[m_mod(x, bufferWidth) + m_mod(z, bufferWidth) * bufferWidth]->chunkRoot != glm::vec3(CHUNKWIDTH * x, 0, CHUNKWIDTH * z))
				LoadChunkFromFile(x, z);

	//GENERATING TREES
	for (int x = chunkX - drawDistance + 1; x <= chunkX + drawDistance - 1; x++)
		for (int z = chunkZ - drawDistance + 1; z <= chunkZ + drawDistance - 1; z++)
		{
			//std::cout << x << '-' << z << std::endl;
			GenerateTrees(x, z);
		}
	//END GENERATING TREES

	//DRAW CHUNKS
	for (int x = chunkX - drawDistance; x <= chunkX + drawDistance; x++)
		for (int z = chunkZ - drawDistance; z <= chunkZ + drawDistance; z++)
		{
			DrawChunk(x, z);
		}
	//END DRAWING CHUNKS

	//std::cout << "Trees: " << m_chunks[m_xzToChunkIndex()]

	m_bufferNeedsToBeReAssigned = true;
	m_loadingThreadRunning = false;
}

void ChunkManager::DrawChunk(int ax, int az)
{
	Chunk* chunk = m_chunks[m_mod(ax, bufferWidth) + m_mod(az, bufferWidth) * bufferWidth];
	int blocksDrawn = 0;
	for (int y = 0; y < CHUNKHEIGHT; y++)
	{
		for (int z = 0; z < CHUNKWIDTH; z++)
		{
			for (int x = 0; x < CHUNKWIDTH; x++)
			{
				if (chunk->blockIDs[blocksDrawn] > 0)
				{
					//0 - UP - 0
					//1 - FRONT - 6
					//2 - DOWN - 12
					//3 - RIGHT - 18
					//4 - BACK - 24
					//5 - LEFT - 30
					if (x == CHUNKWIDTH - 1)
					{
						if (isTransparent(m_chunks[m_mod(ax + 1, bufferWidth) + m_mod(az, bufferWidth) * bufferWidth]->blockIDs[blocksDrawn + 1 - CHUNKWIDTH], chunk->blockIDs[blocksDrawn]))
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[18], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), RIGHT, Display::SOLID);
					}
					else
					{
						if (isTransparent(chunk->blockIDs[blocksDrawn + 1], chunk->blockIDs[blocksDrawn]))
						{
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[18], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), RIGHT, Display::SOLID);
						}
					}

					if (x == 0)
					{
						if (isTransparent(m_chunks[m_mod(ax - 1, bufferWidth) + m_mod(az, bufferWidth) * bufferWidth]->blockIDs[blocksDrawn - 1 + CHUNKWIDTH], chunk->blockIDs[blocksDrawn]))
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[30], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), LEFT, Display::SOLID);
					}
					else
					{
						if (isTransparent(chunk->blockIDs[blocksDrawn - 1], chunk->blockIDs[blocksDrawn]))
						{
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[30], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), LEFT, Display::SOLID);
						}
					}
					//if (y == 0 || chunk->blockIDs[blocksDrawn - CHUNKWIDTH*CHUNKWIDTH] == 0)
					//	m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[12], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot));

					if (y != 0 && isTransparent(chunk->blockIDs[blocksDrawn - CHUNKWIDTH*CHUNKWIDTH], chunk->blockIDs[blocksDrawn]))
						m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[12], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), DOWN, (chunk->blockIDs[blocksDrawn] != 3) ? Display::SOLID : Display::LIQUID);

					if (y == CHUNKHEIGHT - 1 || isTransparent(chunk->blockIDs[blocksDrawn + CHUNKWIDTH*CHUNKWIDTH], chunk->blockIDs[blocksDrawn]))
						m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[0], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), UP, (chunk->blockIDs[blocksDrawn] != 3) ? Display::SOLID : Display::LIQUID);

					if (z == 0)
					{
						if (isTransparent(m_chunks[m_mod(ax, bufferWidth) + m_mod(az - 1, bufferWidth) * bufferWidth]->blockIDs[blocksDrawn + CHUNKWIDTH * (CHUNKWIDTH - 1)], chunk->blockIDs[blocksDrawn]))
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[6], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), FRONT, Display::SOLID);
					}
					else
					{
						if (isTransparent(chunk->blockIDs[blocksDrawn - CHUNKWIDTH], chunk->blockIDs[blocksDrawn]))
						{
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[6], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), FRONT, Display::SOLID);
						}
					}

					if (z == CHUNKWIDTH - 1)
					{
						if (isTransparent(m_chunks[m_mod(ax, bufferWidth) + m_mod(az + 1, bufferWidth) * bufferWidth]->blockIDs[blocksDrawn - CHUNKWIDTH * (CHUNKWIDTH - 1)], chunk->blockIDs[blocksDrawn]))
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[24], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), BACK, Display::SOLID);
					}
					else
					{
						if (isTransparent(chunk->blockIDs[blocksDrawn + CHUNKWIDTH], chunk->blockIDs[blocksDrawn]))
						{
							m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[24], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot), BACK, Display::SOLID);
						}
					}

					//if (z == CHUNKWIDTH - 1 || chunk->blockIDs[blocksDrawn + CHUNKWIDTH] == 0)
					//	m_display->AppendToDrawBuffer(&(*m_blocks)[chunk->blockIDs[blocksDrawn]]->vertices[24], 6, &(glm::vec3(x, y, z) + chunk->chunkRoot));

					//m_visiblilityArray[blocksDrawn] |= 1 << 0;
				}
				blocksDrawn++;
			}
		}
	}
}

void ChunkManager::LoadWorld()
{
	for (int x = 0; x < bufferWidth; x++)
		for (int z = 0; z < bufferWidth; z++)
		{
			LoadChunkFromFile(x, z);
		}
}

void ChunkManager::LoadChunkFromFile(int x, int z)
{
	std::string path = "world/" + std::to_string(x) + 'x' + std::to_string(z) + ".chf";

	std::ifstream file;

	file.open(path, std::fstream::binary | std::fstream::in);

	if (!file.good())
	{
		file.close();
		SaveChunkToFile(x, z, GenerateChunk(x, z));
		return;
	}

	char* ids = new char[CHUNKSIZE];

	file.read(ids, sizeof(char) * CHUNKSIZE);
	char* treesInput = new char[1];
	file.read(treesInput, sizeof(char));

	if (m_chunks[m_xzToChunkIndex(x, z)] != nullptr)
		delete(m_chunks[m_xzToChunkIndex(x, z)]);
	m_chunks[m_xzToChunkIndex(x, z)] = new Chunk(ids, glm::vec3(CHUNKWIDTH * x, 0, CHUNKWIDTH * z), treesInput[0] == 't' ? true : false);

	file.close();
}

void ChunkManager::SaveChunkToFile(int x, int z, Chunk* chunk)
{
	std::ofstream file;
	file.open("world/" + std::to_string(x) + 'x' + std::to_string(z) + ".chf", std::fstream::binary | std::fstream::out);

	file.write(chunk->blockIDs, sizeof(char) * CHUNKSIZE);
	file.write((chunk->structuresGenerated ? "t" : "f"), sizeof(char));

	file.close();
	//std::cout << "Chunk Saved " << x << 'x' << z << std::endl;
}

Chunk* ChunkManager::GenerateChunk(int x, int z)
{
	char* ids = new char[CHUNKSIZE];
	for (int az = 0; az < CHUNKWIDTH; az++)
	{
		for (int ax = 0; ax < CHUNKWIDTH; ax++)
		{
			double xCoord = 0;
			double zCoord = 0;
			GetCoordsOnMap(x, z, ax, az, xCoord, zCoord);
			double valTemp = (m_mapTemp.GetNoise(xCoord, zCoord) + 1) / 2;
			double valSandArea = m_mapSandArea.GetNoise(xCoord, zCoord);
			double valBeachHeight = m_mapBeachHeight.GetNoise(xCoord, zCoord);

			int groundLevel = GetGroudLevel(xCoord, zCoord);


			for (int ay = 0; ay < CHUNKHEIGHT; ay++)
			{
				if (ay < groundLevel)
				{
					if (valTemp < 0.45)
						ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_GRASSC;
					else if (valTemp < 0.55)
						ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_GRASS0;
					else
						ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_SAND0;

					/*if (ay < SEALEVEL + 2 + valBeachHeight * 4)
					{
					double sandRand = (double)(rand() % 200 - 100) * 0.0003;
					if(valSandArea > 0.03)
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_SAND0;
					else if(valSandArea < -0.03)
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_GRASS0;
					else if(sandRand < valSandArea)
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_SAND0;
					else
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_GRASS0;
					}
					else
					{
					if (valVar > DESERTSTEP)
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_GRASS0;
					else
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_GRASS1;
					}*/

				}
				else if (ay < SEALEVEL)
				{
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_WATER;
				}
				else
					ids[m_xyzToIndex(ax, ay, az)] = Blocks::BLOCK_AIR;
			}
		}
	}

	/*
	for (int az = 0; az < CHUNKWIDTH; az++)
	{
	for (int ax = 0; ax < CHUNKWIDTH; ax++)
	{
	double xCoord = 0;
	double zCoord = 0;
	GetCoordsOnMap(x, z, ax, az, xCoord, zCoord);
	double valTemp = (m_mapTemp.GetNoise(xCoord, zCoord) + 1) / 2;
	double valSandArea = m_mapSandArea.GetNoise(xCoord, zCoord);
	double valBeachHeight = m_mapBeachHeight.GetNoise(xCoord, zCoord);
	int groundLevel = GetGroudLevel(xCoord, zCoord);
	if (ax == 0 && az == 0)
	{
	Structure* structure = m_structures[0];
	for (int i = 0; i < structure->length; i < i++)
	{
	ids[m_xyzToIndex(ax + structure->offsets[i].x, groundLevel + structure->offsets[i].y, az + structure->offsets[i].z)] = structure->ids[i];
	}
	}
	}
	}*/

	delete(m_chunks[m_xzToChunkIndex(x, z)]);

	Chunk* chunk = new Chunk(ids, glm::vec3(CHUNKWIDTH * x, 0, CHUNKWIDTH * z));
	m_chunks[m_xzToChunkIndex(x, z)] = chunk;

	//GenerateTrees(x, z);

	return chunk;
}

int ChunkManager::GetGroudLevel(double x, double z)
{
	double valContinental = m_clamp(m_mapHeightContinental.GetNoise(x, z) + 0.5, 1, 0.02);
	double valMaj = m_clamp(m_mapHeightMaj.GetNoise(x, z) + 0.5, 1, 0.02);
	double valMed = m_clamp((m_mapHeightMed.GetNoise(x, z) + 0.5) / 2, 1, 0);
	double valMin = m_mapHeightMin.GetNoise(x, z) + 0.5;
	double valVar = m_clamp(m_mapVariety.GetNoise(x, z) + 0.48, 1.5, 0);

	int groundLevel =
		INFLUENCE_CONTINENTAL * valContinental +
		valVar * (
			INFLUENCE_MAJ * valMaj +
			INFLUENCE_MED * valMed +
			INFLUENCE_MIN * valMin);

	return groundLevel;
}

void ChunkManager::GenerateTrees(int chunkX, int chunkZ)
{
	if (m_chunks[m_xzToChunkIndex(chunkX, chunkZ)]->structuresGenerated)
	{
		return;
	}

	m_chunks[m_xzToChunkIndex(chunkX, chunkZ)]->structuresGenerated = true;
	SetTreesGeneratedInFile(chunkX, chunkZ);

	double xCoord = 0;
	double zCoord = 0;
	for (int blockX = 0; blockX < 16; blockX++)
		for (int blockZ = 0; blockZ < 16; blockZ++)
		{
			GetCoordsOnMap(chunkX, chunkZ, blockX, blockZ, xCoord, zCoord);
			int groundLevel = GetGroudLevel(xCoord, zCoord);
			if (groundLevel >= SEALEVEL)
			{
				double valTreeDensity = m_mapTreeDensity.GetNoise(xCoord, zCoord) * 20 + 4;
				double valFoliageDensity = m_mapFoliageDensity.GetNoise(xCoord, zCoord) * 20 + 5;
				int random = rand() % 400;
				if (random < valFoliageDensity)
				{
					switch (m_xyzToBlock(chunkX * CHUNKWIDTH + blockX, groundLevel - 1, chunkZ * CHUNKWIDTH + blockZ))
					{
					case Blocks::BLOCK_SAND0:
						break;

					case Blocks::BLOCK_GRASS0:
						random = rand();
						if (random % 4 == 0)
							GenerateStructure(Structures::FLOWER_PURPLE, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						else
							GenerateStructure(Structures::BUSH0, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						break;
					}
				}
				random = rand() % 1000;
				if (random < valTreeDensity)
				{
					switch (m_xyzToBlock(chunkX * CHUNKWIDTH + blockX, groundLevel - 1, chunkZ * CHUNKWIDTH + blockZ))
					{
					case Blocks::BLOCK_SAND0:
						random = rand();
						if (random % 4 == 0)
							GenerateStructure(Structures::CACTUS, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						else
							GenerateStructure(Structures::BUSH_D, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						break;

					case Blocks::BLOCK_GRASS0:
						random = rand();
						if (random % 4 == 0)
							GenerateStructure(Structures::TREE0, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						else if (random % 4 == 1)
							GenerateStructure(Structures::TREE1, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						else if(random % 4 == 2)
							GenerateStructure(Structures::BUSH0, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						else
							GenerateStructure(Structures::TREE2, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						break;
					case Blocks::BLOCK_GRASSC:
						GenerateStructure(Structures::TREE_C, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
						break;
					}
					//GenerateStructure(Structures::CACTUS, chunkX * CHUNKWIDTH + blockX, groundLevel, chunkZ * CHUNKWIDTH + blockZ);
					//std::cout << random << '-' << valTreeDensity << std::endl;
				}
			}

		}
}

void ChunkManager::GenerateStructure(int type, int globalX, int globalY, int globalZ)
{
	std::string oldPath = "000";
	Structure* structure = m_structures[type];
	std::fstream file;

	for (int i = 0; i < structure->length; i < ++i)
	{
		m_xyzToBlock(globalX + structure->offsets[i].x, globalY + structure->offsets[i].y, globalZ + structure->offsets[i].z) = structure->ids[i];

		int chunkX = floor((double)(globalX + structure->offsets[i].x) / CHUNKWIDTH);
		int chunkZ = floor((double)(globalZ + structure->offsets[i].z) / CHUNKWIDTH);

		int blockX = m_mod(globalX + structure->offsets[i].x, CHUNKWIDTH);
		int blockZ = m_mod(globalZ + structure->offsets[i].z, CHUNKWIDTH);
		int blockY = globalY + structure->offsets[i].y;

		std::string path = "world/" + std::to_string(chunkX) + 'x' + std::to_string(chunkZ) + ".chf";

		if (path != oldPath)
		{
			if (file.is_open())
				file.close();

			file.open(path, std::fstream::binary | std::fstream::out | std::fstream::in);
		}
		//file.open(path, std::fstream::binary | std::fstream::out | std::fstream::in);
		char writeBuffer[] = { structure->ids[i] };

		file.seekp(m_xyzToIndex(blockX, blockY, blockZ));
		file.write(writeBuffer, sizeof(char));

		oldPath = path;
		//ReplaceBlockInFile((char)type, globalX + structure->offsets[i].x, globalY + structure->offsets[i].y, globalZ + structure->offsets[i].z);
	}
}

void ChunkManager::ReplaceBlockInFile(char c, int globalX, int globalY, int globalZ)
{
	///USELESS
	int chunkX = floor((double)globalX / CHUNKWIDTH);
	int chunkZ = floor((double)globalZ / CHUNKWIDTH);

	int blockX = m_mod(globalX, CHUNKWIDTH);
	int blockZ = m_mod(globalZ, CHUNKWIDTH);
	int blockY = globalY;

	std::string path = "world/" + std::to_string(chunkX) + 'x' + std::to_string(chunkZ) + ".chf";

	std::fstream file;
	file.open(path, std::fstream::binary | std::fstream::out | std::fstream::in);
	char writeBuffer[] = { c };


	file.seekp(m_xyzToIndex(blockX, blockY, blockZ));
	file.write(writeBuffer, sizeof(char));
	file.close();
}

void ChunkManager::SetTreesGeneratedInFile(int chunkX, int chunkZ)
{
	std::string path = "world/" + std::to_string(chunkX) + 'x' + std::to_string(chunkZ) + ".chf";

	std::fstream file;
	file.open(path, std::fstream::binary | std::fstream::out | std::fstream::in);
	char writeBuffer[] = { 't' };
	file.seekp(CHUNKWIDTH*CHUNKWIDTH*CHUNKHEIGHT);
	file.write(writeBuffer, sizeof(char));
	file.close();
}

bool ChunkManager::isTransparent(int idOther, int idThis)
{
	if (idOther == Blocks::BLOCK_FLOWER_PURPLE || idOther == Blocks::BLOCK_CACTUS || idOther == Blocks::BLOCK_BUSH0 || idOther == Blocks::BLOCK_BUSH1)
		return true;

	if (idOther != idThis && (idOther == Blocks::BLOCK_AIR || idOther == Blocks::BLOCK_WATER))
		return true;
	else
		return false;
}