#include "Hamjet/Engine.hpp"
#include "Hamjet/ImageLoader.hpp"
#include "Hamjet/Memory.hpp"
#include "Hamjet/NeuralNet.hpp"

#include <random>
#include <memory>
#include <list>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define GRID_WIDTH 7
#define GRID_HEIGHT 14

#define GRID_X 30
#define GRID_Y 30

#define TILE_SCALE 2

struct Shape {
	int8_t pieces[3][2];
	int nextShape;
};

struct ShapeClass {
	int shapeCount;
	Shape* shapes;
};

struct GameShapes {
	const int allShapeCount = 6;
	Shape shapes[6] = {
		{ {{ -1,0 },{ 0,0 },{ 1, 0 }}, 1},
		{ {{ 0,-1 },{ 0,0 },{ 0, 1 }}, 0},

		{ {{ 0,-1 },{ 0,0 },{ 1, 0 }}, 3},
		{ {{ 1, 0 },{ 0,0 },{ 0, 1 }}, 4},
		{ {{ 0, 1 },{ 0,0 },{ -1,0 }}, 5},
		{ {{ -1,0 },{ 0,0 },{ 0,-1 }}, 2}
	};
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
	int score;

public:
	GameGrid(unsigned int seed) : gameSeed(seed), score(0), lastLowestRow(GRID_HEIGHT) {
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

		currentShape = piece;
		currentShapeX = 3;
		currentShapeY = 1;

		traceShape(movingGrid, piece, 3, 1, 1);

		return true;
	}

	void traceShape(uint8_t grid[GRID_HEIGHT][GRID_WIDTH], int piece, int x, int y, uint8_t val) {
		Shape* s = &shapes.shapes[piece];
		for (int i = 0; i < 3; i++) {
			int nx = x + s->pieces[i][0];
			int ny = y + s->pieces[i][1];
			grid[ny][nx] = val;
		}
	}

	bool canFitPiece(int piece, int x, int y) {
		Shape* s = &shapes.shapes[piece];
		for (int i = 0; i < 3; i++) {
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

			int rowsCleared;
			int lowestRow;
			clearFullRows(&rowsCleared, &lowestRow);

			int rowsAdded = lastLowestRow - lowestRow;
			lastLowestRow = lowestRow;

			score += rowsCleared * 10;
			if (rowsAdded == 0) {
				score += 3;
			} else if (rowsAdded == 1) {
				score += 1;
			}
			printf("New Score: %d\n", score);

			return addNextPiece();
		} else {
			return true;
		}
	}
};

class GameSimulator {
public:
	const int numInputs = GRID_WIDTH * GRID_HEIGHT;
	const int numOutputs = 3;

	GameGrid gg;
	std::shared_ptr<Hamjet::NeuralNet> net;

	bool leftState;
	bool rightState;
	bool rotateState;

public:
	GameSimulator(int seed, std::shared_ptr<Hamjet::NeuralNet> n) : gg(seed), net(n),
		leftState(false), rightState(false), rotateState(false) { }

	void simulateStep() {
		fillInputs();
		net->stepNetwork();

		bool leftDown = net->getNode(numInputs + 0)->value() > 0;
		if (leftDown && !leftState) {
			gg.moveLeft();
		}
		leftState = leftDown;

		bool rightDown = net->getNode(numInputs + 1)->value() > 0;
		if (rightDown && !rightState) {
			gg.moveRight();
		}
		rightState = rightDown;

		bool rotateDown = net->getNode(numInputs + 2)->value() > 0;
		if (rotateDown && !rotateState) {
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

class Gene {
public:
	int innovationNumber;
	int nodeFrom;
	int nodeTo;
	float weight;
	bool disabled;
};

class Genome {
public:
	int fitness;

	int numNodes;
	std::vector<Gene> genes;

public:
	std::shared_ptr<Hamjet::NeuralNet> buildNeuralNet() {
		auto net = std::make_shared<Hamjet::NeuralNet>();

		int numInputs = GRID_HEIGHT * GRID_WIDTH;
		int numOutputs = 10;

		for (int i = 0; i < numInputs + numOutputs; i++) {
			auto n = std::make_shared<Hamjet::NeuralNetNode>();
			net->addNode(n);
		}

		net->getNode(GRID_HEIGHT * GRID_WIDTH)->connect(std::make_unique<Hamjet::NeuralNetConnection>(net->getNode(3), -1.0f));

		return net;
	}
};

class NeatEvolver {
public:
	const int generationSize = 150;

	std::list<std::shared_ptr<Genome>> generation;

public:
	NeatEvolver() {
		firstGeneration();
	}

	void firstGeneration() {
		for (int i = 0; i < generationSize; i++) {
			generation.push_back(std::make_shared<Genome>());
		}
	}

	std::shared_ptr<Genome> processGeneration() {
		std::shared_ptr<Genome> winner;

		for (auto& g : generation) {
			GameSimulator s(0, g->buildNeuralNet());
			g->fitness = s.runEntireGame();

			if (!winner.get() || winner->fitness < g->fitness) {
				winner = g;
			}
		}

		return winner;
	}
};




class NeatrisApp : public Hamjet::Application {
private:
	Hamjet::Engine* engine;

	Hamjet::SDL_Surface_Ptr tileSurface;
	Hamjet::SDL_Texture_Ptr tileTexture;

	std::unique_ptr<GameSimulator> simulator;

	NeatEvolver evolver;

	uint32_t simulationPeriod = 100;
	uint32_t lastSimTime = 0;

public:
	NeatrisApp(Hamjet::Engine* e) : engine(e),
		tileSurface(Hamjet::SDL_Surface_Ptr(Hamjet::ImageLoader::loadPng("assets/tile.png"), SDL_FreeSurface)),
		tileTexture(Hamjet::SDL_Texture_Ptr(SDL_CreateTextureFromSurface(e->windowRenderer, tileSurface.get()), SDL_DestroyTexture)) {
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
		simulator = std::make_unique<GameSimulator>(0, winner->buildNeuralNet());
	}

	int scoreNet(Genome& genome) {
		GameSimulator sim(0, genome.buildNeuralNet());
		return sim.runEntireGame();
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
