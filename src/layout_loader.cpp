#include "runevault/layout_loader.h"

#include "runevault/cell.h"
#include "runevault/fragment_grade.h"
#include "runevault/guarded_item.h"
#include "runevault/item.h"
#include "runevault/scroll_type.h"

#include <fstream>
#include <queue>
#include <sstream>
#include <vector>

namespace runevault {

namespace {

bool isBlankLine(const std::string &line) {
    for (char c : line) {
        if (c != ' ' && c != '\t' && c != '\r') {
            return false;
        }
    }
    return true;
}

// Split `contents` into lines, stripping a trailing '\r' so CRLF files parse
// identically to LF files.
std::vector<std::string> splitLines(const std::string &contents) {
    std::vector<std::string> lines;
    std::istringstream in(contents);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

CellType glyphToCell(char glyph, bool &ok) {
    ok = true;
    switch (glyph) {
    case '|':
    case '-':
        return CellType::Wall;
    case '+':
        return CellType::Archway;
    case '#':
        return CellType::Corridor;
    case '.':
        return CellType::Floor;
    default:
        ok = false;
        return CellType::Wall;
    }
}

// Flood-fill orthogonally connected floor tiles into chambers.
void assignChambers(Board &board) {
    const int rows = board.rows();
    const int cols = board.cols();
    std::vector<std::vector<bool>> seen(rows, std::vector<bool>(cols, false));
    int nextId = 1;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Position start{r, c};
            if (seen[r][c] || board.at(start).type != CellType::Floor) {
                continue;
            }
            // BFS over the 4-connected floor component rooted at `start`.
            const int id = nextId++;
            std::queue<Position> frontier;
            frontier.push(start);
            seen[r][c] = true;
            while (!frontier.empty()) {
                Position p = frontier.front();
                frontier.pop();
                board.setCell(p, CellType::Floor, id);

                const Position steps[] = {{p.row - 1, p.col},
                                          {p.row + 1, p.col},
                                          {p.row, p.col - 1},
                                          {p.row, p.col + 1}};
                for (const Position &n : steps) {
                    if (n.row < 0 || n.row >= rows || n.col < 0 ||
                        n.col >= cols) {
                        continue;
                    }
                    if (seen[n.row][n.col]) {
                        continue;
                    }
                    if (board.at(n).type == CellType::Floor) {
                        seen[n.row][n.col] = true;
                        frontier.push(n);
                    }
                }
            }
        }
    }
}

LoadedLayout fail(const std::string &message) {
    LoadedLayout result;
    result.ok = false;
    result.error = message;
    return result;
}

} // namespace

std::unique_ptr<Item> LayoutLoader::makeItemForDigit(char digit) {
    switch (digit) {
    case '0':
        return std::make_unique<Scroll>(ScrollType::AttuneFocus);
    case '1':
        return std::make_unique<Scroll>(ScrollType::SurgePower);
    case '2':
        return std::make_unique<Scroll>(ScrollType::FortifyWard);
    case '3':
        return std::make_unique<Scroll>(ScrollType::DrainFocus);
    case '4':
        return std::make_unique<Scroll>(ScrollType::SapPower);
    case '5':
        return std::make_unique<Scroll>(ScrollType::ErodeWard);
    case '6':
        return std::make_unique<RuneFragment>(FragmentGrade::CommonShard);
    case '7':
        return std::make_unique<RuneFragment>(FragmentGrade::ResonantShard);
    case '8':
        return std::make_unique<RuneFragment>(FragmentGrade::LichCache);
    case '9':
        return std::make_unique<ShardCoffer>();
    default:
        return nullptr;
    }
}

LoadedLayout LayoutLoader::loadFromString(const std::string &contents) {
    std::vector<std::string> lines = splitLines(contents);

    std::size_t begin = 0;
    std::size_t end = lines.size();
    while (begin < end && isBlankLine(lines[begin])) {
        ++begin;
    }
    while (end > begin && isBlankLine(lines[end - 1])) {
        --end;
    }

    if (begin == end) {
        return fail("layout file is empty: expected a 79x25 map grid");
    }

    const std::vector<std::string> grid(lines.begin() + begin,
                                        lines.begin() + end);

    if (static_cast<int>(grid.size()) > kPlayableRows) {
        return fail("layout file malformed: map has " +
                    std::to_string(grid.size()) + " rows (max " +
                    std::to_string(kPlayableRows) + ")");
    }

    Board board; // all-Wall board at the full playable region size.
    std::vector<ItemPlacement> placements;

    for (int r = 0; r < static_cast<int>(grid.size()); ++r) {
        const std::string &row = grid[r];

        if (static_cast<int>(row.size()) > kPlayableCols) {
            return fail("layout file malformed: row " + std::to_string(r + 1) +
                        " is " + std::to_string(row.size()) +
                        " cols wide (max " + std::to_string(kPlayableCols) +
                        ")");
        }

        for (int c = 0; c < static_cast<int>(row.size()); ++c) {
            const char glyph = row[c];
            const Position p{r, c};

            if (glyph == ' ') {
                // Void outside any level — leave as the default Wall.
                continue;
            }
            if (glyph >= '0' && glyph <= '9') {
                board.setCell(p, CellType::Floor);
                placements.push_back(ItemPlacement{p, glyph});
                continue;
            }

            bool glyphOk = false;
            const CellType type = glyphToCell(glyph, glyphOk);
            if (!glyphOk) {
                return fail(std::string("layout file malformed: row ") +
                            std::to_string(r + 1) + " col " +
                            std::to_string(c + 1) +
                            " has unrecognized glyph '" + glyph + "'");
            }
            board.setCell(p, type);
        }
    }

    // Derive chamber membership by flood-fill.
    assignChambers(board);

    // Replicate the single parsed grid across all five levels.
    LoadedLayout result;
    for (int levelIndex = 0; levelIndex < kLayoutLevelCount; ++levelIndex) {
        result.boards[levelIndex] = board;
        result.items[levelIndex] = placements;
    }
    result.ok = true;
    return result;
}

LoadedLayout LayoutLoader::loadFromFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return fail("layout file could not be opened (missing?): " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return loadFromString(buffer.str());
}

std::unique_ptr<Level> LayoutLoader::buildLevel(const LoadedLayout &loaded,
                                                int levelIndex) {
    if (!loaded.ok || levelIndex < 0 ||
        levelIndex >= kLayoutLevelCount) {
        return nullptr;
    }

    // Copy the parsed board into a fresh Level.
    auto level = std::make_unique<Level>(loaded.boards[levelIndex]);

    for (const ItemPlacement &placement : loaded.items[levelIndex]) {
        std::unique_ptr<Item> item = makeItemForDigit(placement.digit);
        if (!item) {
            continue; // defensive; placements only carry validated digits.
        }
        item->setPosition(placement.position);
        level->addItem(std::move(item));
    }

    return level;
}

} // namespace runevault
