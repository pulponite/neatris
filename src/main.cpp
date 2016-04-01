#include "Hamjet/Engine.hpp"
#include "Hamjet/ImageLoader.hpp"

#include <random>

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
		}
	}
};

class NeatrisApp : public Hamjet::Application {
private:
	Hamjet::Engine* engine;

	SDL_Surface* tileSurface;
	SDL_Texture* tileTexture;

	GameGrid* grid;

	uint32_t simulationPeriod = 50;
	uint32_t lastSimTime = 0;
	uint32_t simsPerTick = 2;
	uint32_t simsSinceTick = 0;

public:
	NeatrisApp(Hamjet::Engine* e) : engine(e), grid(NULL) {
		tileSurface = Hamjet::ImageLoader::loadPng("assets/tile.png");
		tileTexture = SDL_CreateTextureFromSurface(engine->windowRenderer, tileSurface);
		grid = new GameGrid(0);
	}

	~NeatrisApp() {
		SDL_DestroyTexture(tileTexture);
		SDL_FreeSurface(tileSurface);
	}

	virtual bool update(float dt) {
		uint32_t now = SDL_GetTicks();

		if (now - lastSimTime > simulationPeriod) {
			sim();
			lastSimTime = now;
			simsSinceTick++;

			if (simsSinceTick == simsPerTick) {
				tick();
				simsSinceTick = 0;
			}
		}

		return true;
	}

	virtual void draw() {
		SDL_SetRenderDrawColor(engine->windowRenderer, 0, 0, 0, 255);
		SDL_RenderClear(engine->windowRenderer);

		drawStaticGrid();
		if (grid != NULL) {
			drawDynamicGrid(grid->fixedGrid);
			drawDynamicGrid(grid->movingGrid);
		}

		SDL_RenderPresent(engine->windowRenderer);
	}

	virtual void onClick(int x, int y) {
		grid->rotate();
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
		SDL_SetTextureColorMod(tileTexture, 255, 255, 255);
		for (int y = 0; y < GRID_HEIGHT; y++) {
			for (int x = 0; x < GRID_WIDTH; x++) {
				if (grid[y][x] != 0) {
					fillGridPosition(&dest, x, y);
					SDL_RenderCopy(engine->windowRenderer, tileTexture, NULL, &dest);
				}
			}
		}
	}

	void drawStaticGrid() {
		SDL_Rect dest;

		SDL_SetTextureColorMod(tileTexture, 128, 128, 128);

		for (int y = 0; y <= GRID_HEIGHT; y++) {
			fillGridPosition(&dest, -1, y);
			SDL_RenderCopy(engine->windowRenderer, tileTexture, NULL, &dest);
			fillGridPosition(&dest, GRID_WIDTH, y);
			SDL_RenderCopy(engine->windowRenderer, tileTexture, NULL, &dest);
		}

		for (int x = 0; x < GRID_WIDTH; x++) {
			fillGridPosition(&dest, x, GRID_HEIGHT);
			SDL_RenderCopy(engine->windowRenderer, tileTexture, NULL, &dest);
		}
	}

	void fillGridPosition(SDL_Rect* rect, int x, int y) {
		rect->x = GRID_X + (tileSurface->w * TILE_SCALE * x);
		rect->y = GRID_Y + (tileSurface->h * TILE_SCALE * y);
		rect->w = tileSurface->w * TILE_SCALE;
		rect->h = tileSurface->h * TILE_SCALE;
	}

	void sim() {

	}

	void tick() {
		if (!grid->tick()) {
			delete grid;
			grid = new GameGrid(0);
		}
	}
};

int main(int argc, char** argv) {
	Hamjet::Engine engine;

	if (!engine.init(WINDOW_WIDTH, WINDOW_HEIGHT)) {
		engine.term();
		return 1;
	}

	NeatrisApp* app = new NeatrisApp(&engine);
	engine.run(app);
	delete app;

	engine.term();
	return 0;
}
