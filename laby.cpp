// memory_game.cpp
#include <windows.h>
#include <tchar.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>

using namespace std;

#define IDT_SEQUENCE_TIMER   1
#define IDT_HIGHLIGHT_TIMER  2
#define WM_CELL_CLICKED      (WM_APP + 1)

enum GameState { WAIT_START, SHOW_SEQUENCE, PLAYER_GUESS, GAME_OVER };

struct GameData {
    int gridSize;                   // rozmiar planszy NxN
    vector<HWND> cells;             // uchwyty do komórek
    vector<bool> cellHighlight;     // czy dana komórka ma być "podświetlona"
    vector<int> sequence;           // indeksy komórek tworzące sekwencję
    int guessIndex;                 // indeks następnej komórki, którą gracz ma kliknąć
    GameState state;                // aktualny stan gry
    int currentScore;               // aktualny wynik (długość sekwencji)
    int bestScore;                  // najlepszy wynik gracza
    int timerStep;                  // krok timera przy wyświetlaniu sekwencji
    int lastClickedCell;            // indeks ostatnio klikniętej komórki (do krótkiego podświetlenia)
};

GameData g_game;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CellWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Aktualizuje tytuł głównego okna w zależności od stanu gry
void UpdateTitle(HWND hwnd)
{
    TCHAR title[256];
    LPCTSTR stateMsg = _T("");
    switch(g_game.state)
    {
        case WAIT_START:   stateMsg = _T("Naciśnij ESC, aby rozpocząć!"); break;
        case SHOW_SEQUENCE: stateMsg = _T("Zapamiętaj!"); break;
        case PLAYER_GUESS:  stateMsg = _T("Zgadnij!"); break;
        case GAME_OVER:     stateMsg = _T("Błędnie! ESC aby zrestartować!"); break;
    }
    _stprintf_s(title, _T("Score: %d, %s"), g_game.currentScore, stateMsg);
    SetWindowText(hwnd, title);
}

// Tworzy okna-komórki w głównym oknie
void CreateCells(HWND hwnd)
{
    g_game.cells.clear();
    g_game.cellHighlight.assign(g_game.gridSize * g_game.gridSize, false);
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    for (int row = 0; row < g_game.gridSize; row++) {
        for (int col = 0; col < g_game.gridSize; col++) {
            int x = col * 100 + 5;
            int y = row * 100 + 5;
            int cellIndex = row * g_game.gridSize + col;
            HWND hwndCell = CreateWindowEx(
                0,
                _T("MemoryGameCell"),
                NULL,
                WS_CHILD | WS_VISIBLE,
                x, y, 90, 90,
                hwnd,
                (HMENU)cellIndex,
                hInst,
                NULL
            );
            // Zapisujemy indeks w danych okna
            SetWindowLongPtr(hwndCell, GWLP_USERDATA, (LONG_PTR)cellIndex);
            g_game.cells.push_back(hwndCell);
        }
    }
}

// Resetuje dane gry
void ResetGame(HWND hwnd)
{
    g_game.sequence.clear();
    g_game.currentScore = 0;
    g_game.guessIndex = 0;
    g_game.timerStep = 0;
    g_game.state = WAIT_START;
    UpdateTitle(hwnd);
}

// Rozpoczyna grę – tworzy początkową sekwencję i startuje animację
void StartGame(HWND hwnd)
{
    ResetGame(hwnd);
    int totalCells = g_game.gridSize * g_game.gridSize;
    int randIndex = rand() % totalCells;
    g_game.sequence.push_back(randIndex);
    g_game.currentScore = 1;
    UpdateTitle(hwnd);
    g_game.timerStep = 0;
    g_game.state = SHOW_SEQUENCE;
    UpdateTitle(hwnd);
    SetTimer(hwnd, IDT_SEQUENCE_TIMER, 1000, NULL);
}

// Po poprawnym powtórzeniu sekwencji – rozszerza sekwencję o nowy losowy element
void ExtendSequence(HWND hwnd)
{
    int totalCells = g_game.gridSize * g_game.gridSize;
    int randIndex = rand() % totalCells;
    g_game.sequence.push_back(randIndex);
    g_game.currentScore = (int)g_game.sequence.size();
    g_game.guessIndex = 0;
    g_game.timerStep = 0;
    g_game.state = SHOW_SEQUENCE;
    UpdateTitle(hwnd);
    SetTimer(hwnd, IDT_SEQUENCE_TIMER, 1000, NULL);
}

// Procedura głównego okna
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_CREATE:
        CreateCells(hwnd);
        break;
    case WM_KEYDOWN:
        if(wParam == VK_ESCAPE)
        {
            // ESC – rozpoczęcie lub restart gry
            if(g_game.state == WAIT_START || g_game.state == GAME_OVER)
            {
                StartGame(hwnd);
            }
        }
        break;
    case WM_TIMER:
        if(wParam == IDT_SEQUENCE_TIMER)
        {
            // Animacja sekwencji: co sekundę podświetlamy kolejne pole
            if(g_game.timerStep > 0)
            {
                int prevIndex = g_game.timerStep - 1;
                if(prevIndex < (int)g_game.sequence.size())
                {
                    g_game.cellHighlight[g_game.sequence[prevIndex]] = false;
                    InvalidateRect(g_game.cells[g_game.sequence[prevIndex]], NULL, TRUE);
                }
            }
            if(g_game.timerStep < (int)g_game.sequence.size())
            {
                int cellIndex = g_game.sequence[g_game.timerStep];
                g_game.cellHighlight[cellIndex] = true;
                InvalidateRect(g_game.cells[cellIndex], NULL, TRUE);
                g_game.timerStep++;
            }
            else
            {
                // Koniec animacji sekwencji – przechodzimy do etapu zgadywania
                KillTimer(hwnd, IDT_SEQUENCE_TIMER);
                g_game.state = PLAYER_GUESS;
                g_game.guessIndex = 0;
                UpdateTitle(hwnd);
            }
        }
        else if(wParam == IDT_HIGHLIGHT_TIMER)
        {
            // Timer do krótkiego podświetlenia klikniętego pola
            KillTimer(hwnd, IDT_HIGHLIGHT_TIMER);
            int idx = g_game.lastClickedCell;
            g_game.cellHighlight[idx] = false;
            InvalidateRect(g_game.cells[idx], NULL, TRUE);
            // Jeśli gracz poprawnie powtórzył całą sekwencję – rozszerzamy ją
            if(g_game.guessIndex == (int)g_game.sequence.size())
            {
                ExtendSequence(hwnd);
            }
        }
        break;
    case WM_CELL_CLICKED:
        if(g_game.state == PLAYER_GUESS)
        {
            int clickedIndex = (int) lParam; // przekazany indeks komórki
            if(clickedIndex == g_game.sequence[g_game.guessIndex])
            {
                // Poprawny strzał – podświetlamy klikniętą komórkę
                g_game.cellHighlight[clickedIndex] = true;
                InvalidateRect(g_game.cells[clickedIndex], NULL, TRUE);
                g_game.lastClickedCell = clickedIndex;
                g_game.guessIndex++;
                // Jeśli kliknięto ostatnią komórkę w sekwencji,
                // ustaw timer na 1000 ms (pole pozostaje podświetlone przez 1 sekundę)
                if(g_game.guessIndex == (int)g_game.sequence.size())
                    SetTimer(hwnd, IDT_HIGHLIGHT_TIMER, 1000, NULL);
                else
                    SetTimer(hwnd, IDT_HIGHLIGHT_TIMER, 500, NULL);
            }
            else
            {
                // Błędny wybór – gra kończy się
                g_game.state = GAME_OVER;
                UpdateTitle(hwnd);
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Procedura okna-komórki (child window)
// Każda komórka rysuje się jako kwadrat o kolorze #7C0A02 lub, gdy "podświetlona", jako biały
LRESULT CALLBACK CellWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            int index = (int)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            HBRUSH brush;
            if(index >= 0 && index < (int)g_game.cellHighlight.size() && g_game.cellHighlight[index])
                // Podświetlona – np. biały
                brush = CreateSolidBrush(RGB(255,255,255));
            else
                // Normalny kolor komórki: #7C0A02
                brush = CreateSolidBrush(RGB(124,10,2));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &ps);
        }
        break;
    case WM_LBUTTONUP:
        {
            // Przy kliknięciu pobieramy indeks komórki i wysyłamy do okna rodzica komunikat WM_CELL_CLICKED
            int index = (int)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            HWND hwndParent = GetParent(hwnd);
            PostMessage(hwndParent, WM_CELL_CLICKED, 0, (LPARAM)index);
        }
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Funkcja WinMain – punkt wejścia aplikacji
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // Inicjalizacja generatora liczb losowych
    srand((unsigned)time(NULL));

    // Parsowanie argumentu – rozmiar planszy (N od 3 do 10, domyślnie 3)
    int gridSize = 3;
    if(lpCmdLine && lpCmdLine[0] != '\0')
    {
        gridSize = _ttoi(_T(lpCmdLine));
        if(gridSize < 3 || gridSize > 10)
            gridSize = 3;
    }
    g_game.gridSize = gridSize;
    g_game.state = WAIT_START;
    g_game.currentScore = 0;
    g_game.bestScore = 0;

    // Rejestracja klasy głównego okna
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    // Kolor tła głównego okna: #FFFDD0
    wc.hbrBackground = CreateSolidBrush(RGB(255,253,208));
    wc.lpszClassName = _T("MemoryGameMainWindow");
    RegisterClassEx(&wc);

    // Rejestracja klasy okna-komórki
    WNDCLASSEX wcCell = {0};
    wcCell.cbSize        = sizeof(WNDCLASSEX);
    wcCell.style         = CS_HREDRAW | CS_VREDRAW;
    wcCell.lpfnWndProc   = CellWndProc;
    wcCell.hInstance     = hInstance;
    wcCell.hCursor       = LoadCursor(NULL, IDC_ARROW);
    // Kolor tła komórek: #7C0A02
    wcCell.hbrBackground = CreateSolidBrush(RGB(124,10,2));
    wcCell.lpszClassName = _T("MemoryGameCell");
    RegisterClassEx(&wcCell);

    // Obliczenie rozmiaru klienta – każda komórka ma 100x100 (90+10 margines)
    int clientWidth  = gridSize * 100;
    int clientHeight = gridSize * 100;
    RECT rc = {0, 0, clientWidth, clientHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, TRUE);
    int winWidth  = rc.right - rc.left;
    int winHeight = rc.bottom - rc.top;

    // Ustalenie pozycji – wyśrodkowanie ekranu
    int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int winX = (screenWidth - winWidth) / 2;
    int winY = (screenHeight - winHeight) / 2;

    HWND hwndMain = CreateWindowEx(
        0,
        _T("MemoryGameMainWindow"),
        _T("Score: 0, Naciśnij ESC, aby rozpocząć!"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        winX, winY, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );
    if(!hwndMain)
        return 0;

    ShowWindow(hwndMain, nShowCmd);
    UpdateWindow(hwndMain);

    // Główna pętla komunikatów
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
