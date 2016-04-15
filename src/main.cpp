#include "Hamjet/Engine.hpp"
#include "Hamjet/ImageLoader.hpp"
#include "Hamjet/Memory.hpp"
#include "Hamjet/NeuralNet.hpp"
#include "Hamjet/NeatEvolver.hpp"

#include <random>
#include <memory>
#include <list>
#include <set>

#define WINDOW_WIDTH 200
#define WINDOW_HEIGHT 350

#define GRID_WIDTH 7
#define GRID_HEIGHT 14
#define MAX_PIECE_COUNT 5 * 7 * 14

#define GRID_X 30
#define GRID_Y 30

#define TILE_SCALE 2

#define PIECE_COUNT 3
#define NET_ALGO 0

struct Shape {
	int8_t pieces[PIECE_COUNT][2];
	int nextShape;
};

struct ShapeClass {
	int shapeCount;
	Shape* shapes;
};

struct GameShapes {
#if PIECE_COUNT == 1
	const int allShapeCount = 1;
	Shape shapes[1] = {
		{ { { 0,0 } }, 0 }
	};
#elif PIECE_COUNT == 2
	const int allShapeCount = 4;
	Shape shapes[4] = {
		{ { { 0,0 }, { 1,0 } }, 1 },
		{ { { 0,0 },{ 0,1 } }, 2 },
		{ { { 0,0 },{ -1,0 } }, 3 },
		{ { { 0,0 },{ 0,-1 } }, 0 },
	};
#else
	const int allShapeCount = 6;
	Shape shapes[6] = {
		{ {{ -1,0 },{ 0,0 },{ 1, 0 }}, 1},
		{ {{ 0,-1 },{ 0,0 },{ 0, 1 }}, 0},

		{ {{ 0,-1 },{ 0,0 },{ 1, 0 }}, 3},
		{ {{ 1, 0 },{ 0,0 },{ 0, 1 }}, 4},
		{ {{ 0, 1 },{ 0,0 },{ -1,0 }}, 5},
		{ {{ -1,0 },{ 0,0 },{ 0,-1 }}, 2}
	};
#endif
};

class GameGrid {
public:
	uint8_t fixedGrid[GRID_HEIGHT][GRID_WIDTH];
	uint8_t movingGrid[GRID_HEIGHT][GRID_WIDTH];

	GameShapes shapes;

	std::mt19937 twister;
	unsigned int gameSeed;

	int currentShape;
	int currentShapeX;
	int currentShapeY;

	int lastLowestRow;
	int score = 1;

	int pieceCount = 0;

public:
	GameGrid(unsigned int seed) : gameSeed(seed), lastLowestRow(GRID_HEIGHT) {
		for (int y = 0; y < GRID_HEIGHT; y++) {
			for (int x = 0; x < GRID_WIDTH; x++) {
				fixedGrid[y][x] = 0;
				movingGrid[y][x] = 0;
			}
		}

		twister.seed(gameSeed);

		addNextPiece();
	}

	bool addNextPiece() {
		int piece = twister() % shapes.allShapeCount;

		if (!canFitPiece(piece, 3, 1)) {
			return false;
		}

		if (pieceCount >= MAX_PIECE_COUNT) {
			printf("Finished game \\o/\n");
			return false;
		}

		currentShape = piece;
		currentShapeX = 3;
		currentShapeY = 1;

		traceShape(movingGrid, piece, 3, 1, 1);

		pieceCount++;

		return true;
	}

	void traceShape(uint8_t grid[GRID_HEIGHT][GRID_WIDTH], int piece, int x, int y, uint8_t val) {
		Shape* s = &shapes.shapes[piece];
		for (int i = 0; i < PIECE_COUNT; i++) {
			int nx = x + s->pieces[i][0];
			int ny = y + s->pieces[i][1];
			grid[ny][nx] = val;
		}
	}

	bool canFitPiece(int piece, int x, int y) {
		Shape* s = &shapes.shapes[piece];
		for (int i = 0; i < PIECE_COUNT; i++) {
			int nx = x + s->pieces[i][0];
			int ny = y + s->pieces[i][1];

			if (nx < 0 || ny < 0 || nx >= GRID_WIDTH || ny >= GRID_HEIGHT || fixedGrid[ny][nx] == 1) {
				return false;
			}
		}

		return true;
	}

	bool tryNewPosition(int shape, int x, int y) {
		if (canFitPiece(shape, x, y)) {
			traceShape(movingGrid, currentShape, currentShapeX, currentShapeY, 0);

			currentShapeX = x;
			currentShapeY = y;
			currentShape = shape;
			traceShape(movingGrid, currentShape, currentShapeX, currentShapeY, 1);
			return true;
		}
		return false;
	}

	void moveLeft() {
		tryNewPosition(currentShape, currentShapeX - 1, currentShapeY);
	}

	void moveRight() {
		tryNewPosition(currentShape, currentShapeX + 1, currentShapeY);
	}

	void rotate() {
		tryNewPosition(shapes.shapes[currentShape].nextShape, currentShapeX, currentShapeY);
	}

	void clearFullRows(int* rowsCleared, int* lowestRow) {
		int rc = 0;
		int lr = GRID_HEIGHT;

		for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
			bool isFull = true;
			bool hasAny = false;
			for (int x = 0; x < GRID_WIDTH; x++) {
				isFull = isFull && (fixedGrid[y][x] == 1);
				hasAny = hasAny || (fixedGrid[y][x] == 1);
			}

			if (isFull) {
				rc++;
				SDL_memset(fixedGrid[y], 0, GRID_WIDTH);
			}
			else if (rc > 0) {
				SDL_memcpy(fixedGrid[y + rc], fixedGrid[y], GRID_WIDTH);
				SDL_memset(fixedGrid[y], 0, GRID_WIDTH);
			}

			if (hasAny) {
				lr = y + rc;
			}
		}

		*rowsCleared = rc;
		*lowestRow = lr;
	}

	bool tick() {
		if (!tryNewPosition(currentShape, currentShapeX, currentShapeY + 1)) {
			traceShape(fixedGrid, currentShape, currentShapeX, currentShapeY, 1);
			traceShape(movingGrid, currentShape, currentShapeX, currentShapeY, 0);

			int minY = currentShapeY;
			for (int i = 0; i < PIECE_COUNT; i++) {
				if (currentShapeY + shapes.shapes[currentShape].pieces[i][1] < minY) {
					minY = currentShapeY + shapes.shapes[currentShape].pieces[i][1];
				}
			}

			int rowsBelow = minY - lastLowestRow;
			if (rowsBelow < 0) {
				rowsBelow = 0;
			}

			int rowsCleared;
			int lowestRow;
			clearFullRows(&rowsCleared, &lowestRow);

			int rowsAdded = lastLowestRow - lowestRow;
			lastLowestRow = lowestRow;

			score++;
			score += rowsCleared * 10;
			return addNextPiece();
		} else {
			return true;
		}
	}
};

class GameSimulatorBigNet {
public:
	const int numInputs = GRID_WIDTH * GRID_HEIGHT;
	const int numOutputs = 3;

	GameGrid gg;
	std::shared_ptr<Hamjet::NeuralNet> net;

	bool leftState;
	bool rightState;
	bool rotateState;

public:
	GameSimulatorBigNet(int seed, std::shared_ptr<Hamjet::NeuralNet> n) : gg(seed), net(n),
		leftState(false), rightState(false), rotateState(false) { }

	void simulateStep() {
		fillInputs();
		net->stepNetwork();

		auto leftNode = net->getNode(numInputs + 0);
		auto rightNode = net->getNode(numInputs + 1);
		auto rotateNode = net->getNode(numInputs + 2);

		bool leftDown = leftNode->value() > 0.1f;
		if (leftDown/* && !leftState*/) {
			gg.moveLeft();
		}
		leftState = leftDown;

		bool rightDown = rightNode->value() > 0.1f;
		if (rightDown/* && !rightState*/) {
			gg.moveRight();
		}
		rightState = rightDown;

		bool rotateDown = rotateNode->value() > 0.1f;
		if (rotateDown/* && !rotateState*/) {
			gg.rotate();
		}
		rotateState = rotateDown;
	}

	bool tickGame() {
		for (int i = 0; i < 4; i++) {
			simulateStep();
		}
		return gg.tick();
	}

	int runEntireGame() {
		while (tickGame()) {}
		return gg.score;
	}

	void fillInputs() {
		for (int y = 0; y < GRID_HEIGHT; y++) {
			for (int x = 0; x < GRID_WIDTH; x++) {
				int index = y * GRID_WIDTH + x;
				if (gg.fixedGrid[y][x] == 1) {
					net->getNode(index)->setValue(1);
				}
				else if (gg.movingGrid[y][x] == 1) {
					net->getNode(index)->setValue(-1);
				}
			}
		}
	}
};


class NeatGameSimBig : public Hamjet::NeatSimulator {
	virtual int getNumInputs() {
		return GRID_WIDTH * GRID_HEIGHT;
	}

	virtual int getNumOutputs() {
		return 3;
	}

	virtual int evaluateGenome(Hamjet::Genome& g) {
		GameSimulatorBigNet gs(0, g.buildNeuralNet());
		return gs.runEntireGame();
	}
};


class GameSimulatorSmallNet {
public:
	const int numInputs = GRID_WIDTH * GRID_HEIGHT;
	const int numOutputs = 3;

	GameGrid gg;
	std::shared_ptr<Hamjet::NeuralNet> net;

	bool leftState;
	bool rightState;
	bool rotateState;

public:
	GameSimulatorSmallNet(int seed, std::shared_ptr<Hamjet::NeuralNet> n) : gg(seed), net(n),
		leftState(false), rightState(false), rotateState(false) { }

	void simulateStep() {
		fillInputs();
		net->stepNetwork();

		bool leftDown = net->getNode(numInputs + 0)->value() > 0;
		if (leftDown) {
			gg.moveLeft();
		}
		leftState = leftDown;

		bool rightDown = net->getNode(numInputs + 1)->value() > 0;
		if (rightDown) {
			gg.moveRight();
		}
		rightState = rightDown;

		bool rotateDown = net->getNode(numInputs + 2)->value() > 0;
		if (rotateDown) {
			gg.rotate();
		}
		rotateState = rotateDown;
	}

	bool tickGame() {
		for (int i = 0; i < 4; i++) {
			simulateStep();
		}
		return gg.tick();
	}

	int runEntireGame() {
		while (tickGame()) {}
		return gg.score;
	}

	void fillInputs() {
		for (int y = 0; y < GRID_HEIGHT; y++) {
			int adjustedy = (y + gg.currentShapeY) - 2;
			for (int x = 0; x < GRID_WIDTH; x++) {
				int adjustedx = (x + gg.currentShapeX) - 3;
				int inputIndex = y * GRID_WIDTH + x;

				auto node = net->getNode(inputIndex);
				if (adjustedx < 0 || adjustedx >= GRID_WIDTH || adjustedy >= GRID_HEIGHT) {
					node->setValue(1);
				}
				else if (adjustedy < 0) {
					node->setValue(0);
				}
				else {
					if (gg.fixedGrid[adjustedy][adjustedx] == 1) {
						node->setValue(1);
					}
					else if (gg.movingGrid[adjustedy][adjustedx] == 1) {
						node->setValue(-1);
					}
					else {
						node->setValue(0);
					}
				}
			}
		}


		if (gg.currentShapeY == 10) {
			int i = 1;
			i++;
		}
	}
};


class NeatGameSimSmall : public Hamjet::NeatSimulator {
	virtual int getNumInputs() {
		return GRID_WIDTH * GRID_HEIGHT;
	}

	virtual int getNumOutputs() {
		return 3;
	}

	virtual int evaluateGenome(Hamjet::Genome& g) {
		GameSimulatorSmallNet gs(0, g.buildNeuralNet());
		return gs.runEntireGame();
	}
};

#if NET_ALGO == 0
typedef GameSimulatorSmallNet GameSim;
typedef NeatGameSimSmall NeatSim;
#else
typedef GameSimulatorBigNet GameSim;
typedef NeatGameSimBig NeatSim;
#endif



class NeatrisApp : public Hamjet::Application {
private:
	Hamjet::Engine* engine;

	Hamjet::SDL_Surface_Ptr tileSurface;
	Hamjet::SDL_Texture_Ptr tileTexture;

	std::unique_ptr<GameSim> simulator;

	Hamjet::NeatEvolver evolver;

	uint32_t simulationPeriod = 1;
	uint32_t lastSimTime = 0;

public:
	NeatrisApp(Hamjet::Engine* e) : engine(e),
		tileSurface(Hamjet::SDL_Surface_Ptr(Hamjet::ImageLoader::loadPng("assets/tile.png"), SDL_FreeSurface)),
		tileTexture(Hamjet::SDL_Texture_Ptr(SDL_CreateTextureFromSurface(e->windowRenderer, tileSurface.get()), SDL_DestroyTexture)),
		evolver(std::shared_ptr<Hamjet::NeatSimulator>(new NeatSim())) {
		newState();
	}

	virtual bool update(float dt) {
		uint32_t now = SDL_GetTicks();

		if (now - lastSimTime > simulationPeriod) {
			sim();
			lastSimTime = now;
		}

		return true;
	}

	virtual void draw() {
		SDL_SetRenderDrawColor(engine->windowRenderer, 0, 0, 0, 255);
		SDL_RenderClear(engine->windowRenderer);

		drawStaticGrid();
		if (auto sim = simulator.get()) {
			drawDynamicGrid(sim->gg.fixedGrid);
			drawDynamicGrid(sim->gg.movingGrid);
		}

		SDL_RenderPresent(engine->windowRenderer);
	}

	virtual void onClick(int x, int y) {
		return;
	}

	virtual void onKeyDown() {
		/*const uint8_t* keys = SDL_GetKeyboardState(NULL);
		if (keys[SDL_SCANCODE_LEFT]) {
			grid->moveLeft();
		}
		else if (keys[SDL_SCANCODE_RIGHT]) {
			grid->moveRight();
		}
		else if (keys[SDL_SCANCODE_UP]) {
			grid->rotate();
		}
		else if (keys[SDL_SCANCODE_DOWN]) {
			if (!grid->tick()) {
				delete grid;
				grid = new GameGrid(0);
			}
		}*/
		const uint8_t* keys = SDL_GetKeyboardState(NULL);
		if (keys[SDL_SCANCODE_LEFT]) {
			simulationPeriod--;
		}
		else if (keys[SDL_SCANCODE_RIGHT]) {
			simulationPeriod++;
		}
	}

	void drawDynamicGrid(uint8_t grid[GRID_HEIGHT][GRID_WIDTH]) {
		SDL_Rect dest;
		SDL_SetTextureColorMod(tileTexture.get(), 255, 255, 255);
		for (int y = 0; y < GRID_HEIGHT; y++) {
			for (int x = 0; x < GRID_WIDTH; x++) {
				if (grid[y][x] != 0) {
					fillGridPosition(&dest, x, y);
					SDL_RenderCopy(engine->windowRenderer, tileTexture.get(), NULL, &dest);
				}
			}
		}
	}

	void drawStaticGrid() {
		SDL_Rect dest;
		SDL_SetTextureColorMod(tileTexture.get(), 128, 128, 128);

		for (int y = 0; y <= GRID_HEIGHT; y++) {
			fillGridPosition(&dest, -1, y);
			SDL_RenderCopy(engine->windowRenderer, tileTexture.get(), NULL, &dest);
			fillGridPosition(&dest, GRID_WIDTH, y);
			SDL_RenderCopy(engine->windowRenderer, tileTexture.get(), NULL, &dest);
		}

		for (int x = 0; x < GRID_WIDTH; x++) {
			fillGridPosition(&dest, x, GRID_HEIGHT);
			SDL_RenderCopy(engine->windowRenderer, tileTexture.get(), NULL, &dest);
		}
	}

	void fillGridPosition(SDL_Rect* rect, int x, int y) {
		rect->x = GRID_X + (tileSurface->w * TILE_SCALE * x);
		rect->y = GRID_Y + (tileSurface->h * TILE_SCALE * y);
		rect->w = tileSurface->w * TILE_SCALE;
		rect->h = tileSurface->h * TILE_SCALE;
	}

	void sim() {
		if (!simulator->tickGame()) {
			newState();
		}
	}

	void newState() {
		auto winner = evolver.processGeneration();

		printf("Generation %d winner score: %d\n", evolver.generationCount, winner->fitness);
		for (auto gene : winner->genes) {
			printf("[%d:", gene.innovationNumber);

			if (gene.disabled) {
				printf("X:");
			}

			if (gene.nodeFrom < winner->inNodes) {
				printf("i(%d,%d),", gene.nodeFrom % GRID_WIDTH, gene.nodeFrom / GRID_WIDTH);
			}
			else {
				printf("%d,", gene.nodeFrom);
			}

			if (gene.nodeTo < winner->inNodes + winner->outNodes) {
				int out = gene.nodeTo - winner->inNodes;
				if (out == 0) {
					printf("o(Left)]");
				}
				else if (out == 1) {
					printf("o(Right)]");
				}
				else if (out == 2) {
					printf("o(Rotate)]");
				}
			}
			else {
				printf("%d] ", gene.nodeTo);
			}
		}
		printf("\nNext Gen: ");
		for (auto& s : evolver.speciatedGeneration) {
			printf("[%d]", s.genomes.size());
		}
		printf("\n");

		simulator = std::make_unique<GameSim>(0, winner->buildNeuralNet());
	}
};

int main(int argc, char** argv) {
	Hamjet::Engine engine;

	if (!engine.init(WINDOW_WIDTH, WINDOW_HEIGHT)) {
		engine.term();
		return 1;
	}

	{
		std::unique_ptr<NeatrisApp> app = std::make_unique<NeatrisApp>(&engine);
		engine.run(app.get());
	}

	engine.term();
	return 0;
}
