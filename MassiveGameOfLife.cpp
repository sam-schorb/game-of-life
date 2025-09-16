#include <cassert>
#define OLC_PGE_APPLICATION
#include <olcPixelGameEngine.h>

#define OLC_PGEX_TRANSFORMEDVIEW
#include <extensions/olcPGEX_TransformedView.h>

#include <unordered_set>
#include <cassert>

// Fix for missing assert in olcPixelGameEngine
#ifndef assert
#endif

struct HASH_OLC_VI2D {
	std::size_t operator()(const olc::vi2d& v) const
	{
		return int64_t(v.y << sizeof(int32_t) | v.x);
	}
};

class SparseEncodedGOL : public olc::PixelGameEngine
{
public:
	SparseEncodedGOL()
	{
		sAppName = "Huge Game Of Life";
	}

protected:
	std::unordered_set<olc::vi2d, HASH_OLC_VI2D> setActive;
	std::unordered_set<olc::vi2d, HASH_OLC_VI2D> setActiveNext;
	std::unordered_set<olc::vi2d, HASH_OLC_VI2D> setPotential;
	std::unordered_set<olc::vi2d, HASH_OLC_VI2D> setPotentialNext;
	olc::TransformedView tv;
	bool bSimulationRunning = false;
	int nBrushSize = 1;
	olc::vi2d vLastMouseCell = {-999999, -999999};

	void AddCell(const olc::vi2d& cell)
	{
		setActiveNext.insert(cell);
		setActive.insert(cell);
		for (int y = -1; y <= 1; y++)
			for (int x = -1; x <= 1; x++)
				setPotentialNext.insert(cell + olc::vi2d(x, y));
	}

	void DrawWithBrush(const olc::vi2d& center)
	{
		for (int y = -nBrushSize + 1; y < nBrushSize; y++)
			for (int x = -nBrushSize + 1; x < nBrushSize; x++)
				AddCell(center + olc::vi2d(x, y));
	}

protected:
	bool OnUserCreate() override
	{
		tv.Initialise(GetScreenSize());
		// Start with 1:1 scale and centered view for precise clicking
		tv.SetWorldScale({4.0f, 4.0f}); // Start zoomed in for better visibility
		tv.SetWorldOffset({-GetScreenSize().x / 8.0f, -GetScreenSize().y / 8.0f}); // Center the view
		return true;
	}

	int GetCellState(const olc::vi2d& in)
	{
		return setActive.contains(in) ? 1 : 0;
	}

	void CalculateNextGeneration(
		const std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& currentActive,
		const std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& potentialCells,
		std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& nextActive,
		std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& nextPotential)
	{
		auto getCellState = [&currentActive](const olc::vi2d& cell) -> int {
			return currentActive.contains(cell) ? 1 : 0;
		};

		for (const auto& c : potentialCells)
		{
			// Cell has changed, apply rules

			// The secret of artificial life =================================================
			int nNeighbours =
				getCellState(olc::vi2d(c.x - 1, c.y - 1)) +
				getCellState(olc::vi2d(c.x - 0, c.y - 1)) +
				getCellState(olc::vi2d(c.x + 1, c.y - 1)) +
				getCellState(olc::vi2d(c.x - 1, c.y + 0)) +
				getCellState(olc::vi2d(c.x + 1, c.y + 0)) +
				getCellState(olc::vi2d(c.x - 1, c.y + 1)) +
				getCellState(olc::vi2d(c.x + 0, c.y + 1)) +
				getCellState(olc::vi2d(c.x + 1, c.y + 1));


			if (getCellState(c) == 1)
			{
				// if cell is alive and has 2 or 3 neighbours, cell lives on
				int s = (nNeighbours == 2 || nNeighbours == 3);

				if (s == 0)
				{
					// Kill cell through activity omission

					// Neighbours are stimulated for computation next epoch
					for (int y = -1; y <= 1; y++)
						for (int x = -1; x <= 1; x++)
							nextPotential.insert(c + olc::vi2d(x, y));
				}
				else
				{
					// No Change - Keep cell alive
					nextActive.insert(c);
				}


			}
			else
			{
				int s = (nNeighbours == 3);
				if (s == 1)
				{
					// Spawn cell
					nextActive.insert(c);

					// Neighbours are stimulated for computation next epoch
					for (int y = -1; y <= 1; y++)
						for (int x = -1; x <= 1; x++)
							nextPotential.insert(c + olc::vi2d(x, y));
				}
				else
				{
					// No Change - Keep cell dead
				}
			}
			// ===============================================================================
		}
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		// Laptop-friendly controls
		// Arrow keys for panning
		float fPanSpeed = 10.0f / tv.GetWorldScale().x; // Adjust pan speed based on zoom
		if (GetKey(olc::Key::UP).bHeld) tv.MoveWorldOffset({0, -fPanSpeed});
		if (GetKey(olc::Key::DOWN).bHeld) tv.MoveWorldOffset({0, fPanSpeed});
		if (GetKey(olc::Key::LEFT).bHeld) tv.MoveWorldOffset({-fPanSpeed, 0});
		if (GetKey(olc::Key::RIGHT).bHeld) tv.MoveWorldOffset({fPanSpeed, 0});

		// Q and E for zoom (laptop friendly)
		if (GetKey(olc::Key::Q).bPressed) tv.ZoomAtScreenPos(0.9f, GetScreenSize() / 2);
		if (GetKey(olc::Key::E).bPressed) tv.ZoomAtScreenPos(1.1f, GetScreenSize() / 2);

		// Keep mouse wheel zoom for those who have it
		if (GetMouseWheel() > 0) tv.ZoomAtScreenPos(1.1f, GetMousePos());
		if (GetMouseWheel() < 0) tv.ZoomAtScreenPos(0.9f, GetMousePos());

		// Brush size controls - using number keys for reliability
		if (GetKey(olc::Key::K1).bPressed) nBrushSize = 1;
		if (GetKey(olc::Key::K2).bPressed) nBrushSize = 2;
		if (GetKey(olc::Key::K3).bPressed) nBrushSize = 3;
		if (GetKey(olc::Key::K4).bPressed) nBrushSize = 4;
		if (GetKey(olc::Key::K5).bPressed) nBrushSize = 5;
		if (GetKey(olc::Key::K6).bPressed) nBrushSize = 6;
		if (GetKey(olc::Key::K7).bPressed) nBrushSize = 7;
		if (GetKey(olc::Key::K8).bPressed) nBrushSize = 8;
		if (GetKey(olc::Key::K9).bPressed) nBrushSize = 9;
		if (GetKey(olc::Key::K0).bPressed) nBrushSize = 10;

		// Toggle simulation with spacebar
		if (GetKey(olc::Key::SPACE).bPressed) bSimulationRunning = !bSimulationRunning;

		if (bSimulationRunning)
		{

			setActive = setActiveNext;
			setActiveNext.clear();
			setActiveNext.reserve(setActive.size());


			setPotential = setPotentialNext;

			// We know all active cells this epoch have potential to stimulate next epoch
			setPotentialNext = setActive;

			CalculateNextGeneration(setActive, setPotential, setActiveNext, setPotentialNext);
		}


		// Left click/drag to draw cells (works in both paused and running modes)
		if (GetMouse(0).bHeld)
		{
			// Apply offset correction for macOS title bar (approximately 32 pixels)
			auto correctedMousePos = GetMousePos() + olc::vi2d(0, +32);
			auto worldPos = tv.ScreenToWorld(correctedMousePos);
			auto m = olc::vi2d(std::round(worldPos.x), std::round(worldPos.y));

			// Only draw if mouse moved to a new cell (prevents overdrawing)
			if (m != vLastMouseCell)
			{
				DrawWithBrush(m);
				vLastMouseCell = m;
			}
		}
		else if (GetMouse(0).bReleased)
		{
			// Reset last mouse position when released
			vLastMouseCell = {-999999, -999999};
		}

		// Right click or R key to add random cluster (works in both modes)
		if (GetMouse(1).bPressed || GetKey(olc::Key::R).bPressed)
		{
			// Apply offset correction for macOS title bar (approximately 32 pixels)
			auto correctedMousePos = GetMousePos() + olc::vi2d(0, +32);
			auto worldPos = tv.ScreenToWorld(correctedMousePos);
			auto m = olc::vi2d(std::round(worldPos.x), std::round(worldPos.y));

			int clusterSize = nBrushSize * 3;
			for (int i = -clusterSize; i <= clusterSize; i++)
				for (int j = -clusterSize; j < clusterSize; j++)
				{
					if (rand() % 3 == 0) // Reduce density for better patterns
					{
						AddCell(m + olc::vi2d(i, j));
					}
				}
		}

		// Clear cells with C key (only when paused for safety)
		if (!bSimulationRunning && GetKey(olc::Key::C).bPressed)
		{
			setActive.clear();
			setActiveNext.clear();
			setPotential.clear();
			setPotentialNext.clear();
		}


		size_t nDrawCount = 0;
		for (const auto& c : setActive)
		{
			if (tv.IsRectVisible(olc::vf2d(c), olc::vf2d(1, 1)))
			{
				tv.FillRectDecal(olc::vf2d(c), olc::vf2d(1, 1), olc::WHITE);
				nDrawCount++;
			}
		}

		// Status display
		DrawStringDecal({ 2,2 }, "Cells: " + std::to_string(setActiveNext.size()) + " | Potential: " + std::to_string(setPotentialNext.size()) + " | Visible: " + std::to_string(nDrawCount));

		// Debug info - show mouse coordinates (moved down to avoid menu bar)
		auto mousePos = GetMousePos();
		auto correctedMousePos = mousePos + olc::vi2d(0, +32);
		auto worldPos = tv.ScreenToWorld(correctedMousePos);
		auto cellPos = olc::vi2d(std::round(worldPos.x), std::round(worldPos.y));
		DrawStringDecal({ 2,72 }, "Mouse: (" + std::to_string(mousePos.x) + "," + std::to_string(mousePos.y) + ") Corrected: (" + std::to_string(correctedMousePos.x) + "," + std::to_string(correctedMousePos.y) + ") Cell: (" + std::to_string(cellPos.x) + "," + std::to_string(cellPos.y) + ")");

		// Mode indicator (moved below menu bar)
		if (bSimulationRunning)
		{
			DrawStringDecal({ 2,82 }, "MODE: RUNNING (SPACE to pause) - Live editing enabled", olc::GREEN);
		}
		else
		{
			DrawStringDecal({ 2,82 }, "MODE: PAUSED (SPACE to run)", olc::CYAN);
		}

		// Brush info and controls
		DrawStringDecal({ 2,92 }, "Brush Size: " + std::to_string(nBrushSize) + " (1-9,0 keys to change)");
		DrawStringDecal({ 2,102 }, "Controls: Arrows=Pan | Q/E=Zoom | Drag=Paint | R=Random | C=Clear");

		return !GetKey(olc::Key::ESCAPE).bPressed;
	}
};


int main()
{
	SparseEncodedGOL demo;
	if (demo.Construct(1280, 960, 1, 1, false))
		demo.Start();

	return 0;
}