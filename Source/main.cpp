///////////////////////////////////////////////////////////////////////////////////////////////////
//
// STEWARTS CHESS PROGRAMME
//
// Author: Stewart Tunbridge, Pi Micros
// Email:  stewarttunbridge@gmail.com
// Copyright (c) 2024 Stewart Tunbridge, Pi Micros
//
//
// TODO:
// / Check: Undo when playing Black
// / Stats => [White/Black] Stats .... Score = ####{relative to the Stats subject}
// Castle: Still allows castle thru threatened square
//
///////////////////////////////////////////////////////////////////////////////////////////////////


const char Revision [] = "1.81";

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../Widgets/Widgets.hpp"
#include "../Widgets/WidgetsGrid.hpp"
#include "../Widgets/FileSelect.hpp"
#include "../Widgets/ColourSelect.hpp"
#include "../Widgets/MessageBox.hpp"
#include "../Widgets/MessageResponseBox.hpp"

#include "../ConsoleApps/chess/Chess.c"

void DebugAdd (const char* Message)
  {
    static int Time0 = ClockMS ();
    static int Time1 = Time0;
    char *s = (char *) malloc (StrLen (Message) + 32);
    char *ps = s;
    int t;
    //
    t = ClockMS ();
    IntToStrDecimals (&ps, t - Time0, 3);
    StrCat (&ps, " [");
    IntToStrDecimals (&ps, t - Time1, 3);
    Time1 = t;
    StrCat (&ps, "] ");
    StrCat (&ps, Message);
    *ps = 0;
    //
    puts (s);
    //Log ("Log", s);
    free (s);
  }

void CoordToStr (char **s, _Coord Pos)
  {
    *(*s)++ = Pos.x + 'a';
    *(*s)++ = Pos.y + '1';
  }

bool Refresh;
bool Exit;
bool Load;
bool Save;
bool SaveLog;
bool PCPlayForever;

bool PlayThreadWhite;
bool PlayThreadStarted;
bool PlayThreadFinished;
int PlayThreadScore;
int PlayThreadTime;

int PlayThread (void *PlayWhite)
  {
    PlayThreadWhite = (bool) PlayWhite;
    PlayThreadStarted = true;
    PlayThreadFinished = false;
    MovesConsidered = 0;
    PlayThreadTime = ClockMS ();
    PlayThreadScore = BestMove (PlayThreadWhite, 0);
    PlayThreadTime = ClockMS () - PlayThreadTime;
    PlayThreadFinished = true;
    PlayThreadStarted = false;
    return 0;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BOARD VISUAL COMPONENT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

_DirItem *Fonts;   // Directory List of all Font files
int FontPiecesSelect = 0;   // Index into Fonts
char *FontPieces = nullptr;   // Points to Name in Fonts. This never owns the data
int FontPiecesMap = 0;   // Specifies Mapping table (below) to convert _Piece to "character" for FontPieces

// Maps _Piece to character. Multiple mapps apply
int BlackPieceToUChar [3][8] = {
                                 {' ', 'l', 'w', 't', 'n', 'j', 'o'},   // Common map
                                 {' ', 'l', 'w', 't', 'v', 'm', 'o'},   // another "
                                 {' ', 0x265a, 0x265b, 0x265c, 0x265d, 0x265e, 0x265f}   // Unicode standard chess pieces
                               };
int WhitePieceToUChar [3][8] = {
                                 {' ', 'k', 'q', 'r', 'b', 'h', 'p'},
                                 {' ', 'k', 'q', 'r', 'b', 'n', 'p'},
                                 {' ', 0x2654, 0x2655, 0x2656, 0x2657, 0x2658, 0x2659}
                               };

void PieceToChar (char **Dest, _Piece Pce)
  {
    if (PieceWhite (Pce))
      UTF32ToStr (Dest, WhitePieceToUChar [FontPiecesMap][Piece (Pce)]);
    else
      UTF32ToStr (Dest, BlackPieceToUChar [FontPiecesMap][Piece (Pce)]);
  }

void AllPieces (char *Res)
  {
    int p;
    char *r;
    //
    r = Res;
    PieceToChar (&r, PieceFrom (pEmpty, false));
    StrCat (&r, '\t');
    for (p = pEmpty + 1; p <= pPawn; p++)
      {
        PieceToChar (&r, PieceFrom (p, false));
        StrCat (&r, '\t');
      }
    for (p = pEmpty + 1; p <= pPawn; p++)
      {
        PieceToChar (&r, PieceFrom (p, true));
        StrCat (&r, '\t');
      }
    *r = 0;
  }

bool FontsCallBack (_DirItem *Item)
  {
    if (!Item->Directory)
      if (StrPos (Item->Name, ".ttf"))
        return true;
    return false;
  }

int FontsRead (void)
  {
    DirRead (NULL, &Fonts, FontsCallBack);
    Fonts = ListSort (Fonts, DirItemsCompare);
    if (Fonts)
      FontPieces = Fonts->Name;
    return ListLength (Fonts);
  }

class _ChessBoard: public _Container
  {
    public:
      bool RotateAllowed;
      bool Rotate;
      int Colours [2];
      bool MoveStart, MoveComplete;
      bool MoveWhite;
      _Coord Move [2][2];   // Colour * From/To
      bool LegalMovesShow;
      bool LegalMoves [8][8];
      //
      _ChessBoard (_Container *Parent, _Rect Rect);
      void DrawCustom (void);
      bool ProcessEventCustom (_Event *Event, _Point Offset);
      void TranslateCoord (_Coord *Pos);
      void BoardSquare (_Coord Pos, _Rect *Res);
      _Point BoardSquareCenter (_Coord Pos);
      void DrawVector (_Coord Move []);
  };

_ChessBoard::_ChessBoard (_Container *Parent, _Rect Rect) : _Container (Parent, Rect)
  {
    RotateAllowed = true;
    Colours [0] = ColourAdjust (cBrown, 140);
    Colours [1] = ColourAdjust (cBrown, 180);
    MoveStart = MoveComplete = false;
    MoveWhite = false;
    for (int c = 0; c < 2; c++)
      Move [c][0] = Move [c][1] = {-1, -1};
    LegalMovesShow = true;
    memset (LegalMoves, false, sizeof (LegalMoves));
  }

const int cGreenGray = 0x9cf09c;
const int cBlueGray = 0xffaaaa;

void _ChessBoard::TranslateCoord (_Coord *Pos)
  {
    if (Rotate)
      {
        Pos->x = 7 - Pos->x;
        Pos->y = 7 - Pos->y;
      }
  }
void _ChessBoard::BoardSquare (_Coord Pos, _Rect *Res)
  {
    int dx = Rect.Width / 8;
    int dy = Rect.Height / 8;
    TranslateCoord (&Pos);
    *Res = {Pos.x * dx, (7 - Pos.y) * dy, dx, dy};
  }

_Point _ChessBoard::BoardSquareCenter (_Coord Pos)
  {
    int dx = Rect.Width / 8;
    int dy = Rect.Height / 8;
    TranslateCoord (&Pos);
    return {Pos.x * dx + dx / 2, (7 - Pos.y) * dy + dy / 2};
  }

void _ChessBoard::DrawVector (_Coord Move [])
  {
    _Point p0, p1;
    int Width;
    //
    if (Move [0].x >= 0)
      {
        p0 = BoardSquareCenter (Move [0]);
        p1 = BoardSquareCenter (Move [1]);
        Width = Max (1, Rect.Width / 8 / 40);
        DrawLine (p0.x, p0.y, p1.x, p1.y, cBlack, Width);//cGreenGray cGrayDark cGray
        DrawCircle (p0.x, p0.y, 3 * Width, -1, cBlack, 0);//
      }
  }

void _ChessBoard::DrawCustom (void)
  {
    _Rect Square, s;
    _Point Center;
    int x, y, Pass;
    _Piece p;
    char St [8], *c;
    int WidthLine;
    int WidthCross;
    //
    Rotate = !PlayerWhite && RotateAllowed;
    WidthLine = Max (1, Rect.Width / 8 / 24);
    WidthCross = Rect.Width / 8 / 8;   // Half width of cross
    FontSet (FontPieces, (Rect.Height / 8) -2);//* 5 / 6);
    for (Pass = 0; Pass < 2; Pass++)
      {
        if (Pass == 1)
          FontSet (NULL, (Rect.Height / 8) * 3 / 10, fsBold);
        for (y = 0; y < 8; y++)   // for all squares
          for (x = 0; x < 8; x++)
            {
              BoardSquare ({x, y}, &Square);
              Center = BoardSquareCenter ({x, y});
              ColourGrad = Colour = Colours [(x ^ y) & 1];
              if (MoveStart)
                if ((x == Move [MoveWhite][0].x && y == Move [MoveWhite][0].y) || (x == Move [MoveWhite][1].x && y == Move [MoveWhite][1].y))
                  Colour = cWhite;//%%%%
              if (Pass == 0)
                {
                  DrawRectangle (Square, cBlack, cBlack, Colour);   // Background
                  p = Board [x][y];
                  if (Piece (p) != pEmpty)
                    {
                      c = St;   // Piece
                      PieceToChar (&c, p);
                      *c = 0;
                      TextOutAligned (Square, St, aCenter, aCenter);
                    }
                  if (p & pChecked || ((p & pPawn2) && (p / pMoveID == MoveID)))
                    DrawCircle (Center.x, Center.y, Max (Rect.Width / 8 / 16, 1), cRed, ColourAdjust (cRed, 150), 1);
                  if (LegalMovesShow && LegalMoves [x][y])   // Legal moves
                    {
                      DrawLine (Center.x - WidthCross, Center.y - WidthCross, Center.x + WidthCross, Center.y + WidthCross, cWhite, WidthLine);
                      DrawLine (Center.x + WidthCross, Center.y - WidthCross, Center.x - WidthCross, Center.y + WidthCross, cWhite, WidthLine);
                    }
                }
              else   // Pass == 1
                if (Square.Height > 40)   // Large enough for Column / Row markers?
                  {
                    s = AddMargin (Square, {2, 1});
                    ColourText = Colours [(x ^ y) & 1 ^ 1];   // Write in other square colour
                    St [1] = 0;   // terminate string before it's made
                    if (x == 0)
                      {
                        St [0] = y + '1';
                        TextOutAligned (s, St, Rotate ? aRight : aLeft, Rotate ? aBottom : aTop);
                      }
                    if (y == 0)
                      {
                        St [0] = x + 'a';
                        TextOutAligned (s, St, Rotate ? aLeft : aRight, Rotate ? aTop : aBottom);
                      }
                    ColourText = -1;   // back to normal (black)
                    /*if (Controlled [x][y] & 0x01)   // Controlled by Black
                      DrawCircle (Square.x + 4, Square.y + 4, 4, -1, cBlack);
                    if (Controlled [x][y] & 0x02)   // Controlled by White
                      DrawCircle (Square.x + 4, Square.y + 8, 4, -1, cWhite);*/
                  }
            }
      }
    DrawVector (Move [0]);
    DrawVector (Move [1]);
    Colour = -1;   // Revert to transparent
  }

bool _ChessBoard::ProcessEventCustom (_Event *Event, _Point Offset)
  {
    _Coord Sel, Moves [64], *m;
    bool Res;
    _Piece p;
    //
    Res = false;
    if (Event->Type == etMouseDown)//####
      DebugAddHex ("etMouseDown", Event->MouseKeys);//####
    if (IsEventMine (Event, Offset))
      if (Event->Type == etMouseDown && (Event->MouseKeys == (Bit [KeyMouseLeft - 1] | Bit [KeyMouseRight - 1]) || PCPlayForever))
        PCPlayForever = !PCPlayForever;
      else if (!PlayThreadStarted)
        {
          Sel.x = (Event->X - Offset.x) / (Rect.Width / 8);
          Sel.y = 7 - ((Event->Y - Offset.y) / (Rect.Height / 8));
          TranslateCoord (&Sel);
          p = Board [Sel.x][Sel.y];
          if (Event->Type == etMouseDown && Event->Key == KeyMouseLeft)
            {
              if (Piece (p) != pEmpty)
                {
                  MoveStart = true;
                  MoveWhite = PieceWhite (p);
                  Move [MoveWhite][0] = Sel;
                  Move [MoveWhite][1] = Sel;
                  // Store possible moves in MovePos []
                  memset (LegalMoves, false, sizeof (LegalMoves));
                  if (Piece (Board [Sel.x][Sel.y]) != pEmpty)
                    {
                      GetPieceMoves (Sel, Moves, Analysis);
                      m = Moves;
                      while (m->x >= 0)   // for all moves
                        {
                          LegalMoves [m->x][m->y] = true;
                          m++;
                        }
                    }
                  Invalidate (true);
                }
              Res = true;
            }
          else if (Event->Type == etMouseMove && MoveStart)
            {
              if (Sel.x != Move [MoveWhite][1].x || Sel.y != Move [MoveWhite][1].y)
                {
                  Move [MoveWhite][1] = Sel;
                  Invalidate (true);
                }
              Res = true;
            }
          else if (Event->Type == etMouseUp && Event->Key == KeyMouseLeft)
            if (MoveStart)
              {
                if (Move [MoveWhite][0].x == Move [MoveWhite][1].x && Move [MoveWhite][0].y == Move [MoveWhite][1].y)   // we haven't moved
                  MoveStart = false;
                else if (Event->ShiftState & KeyCntl)   // Debug: Set Forbiden move %%%% delete this
                  {
                    MoveForbidenFrom.x = Move [MoveWhite][0].x;
                    MoveForbidenFrom.y = Move [MoveWhite][0].y;
                    MoveForbidenTo.x = Move [MoveWhite][1].x;
                    MoveForbidenTo.y = Move [MoveWhite][1].y;
                    MoveStart = false;
                  }
                else
                  MoveComplete = true;
                memset (LegalMoves, false, sizeof (LegalMoves));
                Invalidate (true);
                Res = true;
              }
        }
    return Res;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GLOBALS
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormMain: public _Form
  {
    public:
      const int LogsSize = 4000;
      const int BoardWidth = 500;
      char *Logs, *sLogs;
      _Piece Graveyard [2][64];
      int ColourBG [2];
      //
      _Container *Toolbar;
      _Button *bProperties, *bUndo, *bPlay, *bRestart, *bEdit, *bHelp, *bQuit;
      _Label *lMessage;
      _Label *lGraveyard;
      _ChessBoard *cBoard;
      _Label *lLogs;
      _Label *lPCStats;
      //
      _Wait *Wait;
      _MenuPopup *Menu;
      _MenuPopup *mPieces;
      //
      _FormMain (char *Title);
     ~_FormMain ();
  };

_FormMain *fMain;

bool PCPlays;

bool Undo;
bool PCPlay;
bool Analyse;
bool Restart;

char *Path;

typedef struct
  {
    _Coord From;
    _Coord To;
    _Piece OldFrom;
    _Piece OldTo;
    _SpecialMove SpecMov;
    char *Log;
  } _UndoItem;

_UndoItem UndoStack [1000];

int UndoStackSize;

int StrChangeChar (char *p, char c1, char c2)
  {
    int n;
    //
    n = 0;
    while (*p)
      {
        if (*p == c1)
          {
            *p = c2;
            n++;
          }
        p++;
      }
    return n;
  }

char *RemoveTextFormats (char *St)
  {
    char *Res, *r;
    //
    Res = NULL;
    StrAssignCopy (&Res, St);
    r = Res;
    while (true)
      {
        r = StrPos (r, '\a');
        if (r == NULL)
          break;
        StrDelete (r, 2);
      }
    r = Res;
    while (true)
      {
        r = StrPos (r, '\v');
        if (r == NULL)
          break;
        *r = tab;
      }
    return Res;
  }

bool FileWriteLine_ (int f, char *Line)
  {
    bool Res;
    //
    StrChangeChar (Line, '\n', ',');
    Res = FileWriteLine (f, Line);
    StrChangeChar (Line, ',', '\n');
    return Res;
  }

void GameSave (char *Name, void *Parameter)
  {
    int f;   // file ID
    char *Line, *l;
    int x, y;
    _Piece *p;
    //
    Line = (char *) malloc (fMain->LogsSize + 100);
    f = FileOpen (Name, foWrite);
    if (f >= 0)
      {
        l = Line;
        StrCat (&l, "Board\t");
        for (y = 0; y < 8; y++)
          for (x = 0; x < 8; x++)
            {
              NumToStrBase (&l, Board [x][y], 0, 16);
              StrCat (&l, ',');
            }
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "PlayerWhite\t");
        NumToStr (&l, PlayerWhite);
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "Graveyard\t");
        for (y = 0; y <= 1; y++)
          {
            p = fMain->Graveyard [y];
            while (*p != pEmpty)
              {
                NumToHex (&l, *p++);
                StrCat (&l, ',');
              }
            StrCat (&l, '\t');
          }
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "Logs\t");
        StrCat (&l, fMain->Logs);
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "Moves\t");
        NumToStr (&l, MoveID);
        *l = 0;
        FileWriteLine_ (f, Line);
        FileClose (f);
      }
    else
      StrCat (&fMain->sLogs, "*** ERROR SAVING FILE ***\n");
    free (Line);
  }

void GameLoad (char *Name, void *Parameter)
  {
    int f;   // file ID
    int Size;
    char *Data, *dp, *dp_;
    int x, y;
    _Piece *p;
    int n;
    //
    f = FileOpen (Name, foRead);
    if (f >= 0)
      {
        fMain->Graveyard [0][0] = fMain->Graveyard [1][0] = pEmpty;
        Size = FileSize (f);
        Data = (char *) malloc (Size + 1);
        FileRead (f, (byte *) Data, Size);
        Data [Size] = 0;   // Terminate String
        dp = Data;
        while (*dp)
          {
            // Break off a line
            dp_ = StrPos (dp, '\n');
            *dp_++ = 0;
            // Look for a Data Name
            if (StrMatch (&dp, "Board\t"))
              {
                x = y = 0;
                while (true)
                  {
                    if (*dp == 0)
                      break;
                    Board [x][y] = (_Piece) StrGetHex (&dp);
                    dp++;
                    if (++x == 8)
                      {
                        x = 0;
                        if (++y == 8)
                          break;
                      }
                  }
              }
            else if (StrMatch (&dp, "PlayerWhite\t"))
              PlayerWhite = StrGetNum (&dp);
            else if (StrMatch (&dp, "Graveyard\t"))
              {
                p = fMain->Graveyard [0];
                n = 0;
                while (*dp && n < SIZEARRAY (fMain->Graveyard [0]))
                  {
                    if (*dp == '\t')
                      {
                        p = fMain->Graveyard [1];
                        n = 0;
                        dp++;
                      }
                    if (*dp)
                      {
                        p [n++] = (_Piece) StrGetHex (&dp);
                        p [n] = pEmpty;
                      }
                    if (*dp == ',')
                      dp++;
                  }
              }
            else if (StrMatch (&dp, "Logs\t"))
              {
                strcpy (fMain->Logs, dp);
                StrChangeChar (fMain->Logs, ',', '\n');
              }
            else if (StrMatch (&dp, "Moves\t"))
              MoveID = StrGetNum (&dp);
            else
              break;
            dp = dp_;
          }
        FileClose (f);
        free (Data);
        fMain->sLogs = StrPos (fMain->Logs, (char) 0);
        UndoStackSize = 0;
        fMain->cBoard->Move [0][0].x = -1;
        fMain->cBoard->Move [1][0].x = -1;
        fMain->lMessage->TextSet (NULL);
        fMain->lMessage->VisibleSet (false);
        Refresh = true;
      }
    else
      StrCat (&fMain->sLogs, "*** ERROR LOADING FILE ***\n");
  }

void LogSave (char *Name, void *Parameter)
  {
    int f;   // file ID
    char *l;
    //
    f = FileOpen (Name, foWrite);
    if (f >= 0)
      {
        l = RemoveTextFormats (fMain->Logs);
        FileWrite (f, (byte *) l, StrLen (l));
        FileClose (f);
        free (l);
      }
    else
      StrCat (&fMain->sLogs, "*** ERROR SAVING FILE ***\n");
  }

#define FileSettings ".Chess"

bool SettingsSave (void)
  {
    int f;   // file ID
    char *Line, *l;
    //
    Line = (char *) malloc (250);
    f = FileOpen_ (FileSettings, foWrite);
    if (f >= 0)
      {
        l = Line;
        StrCat (&l, "Depth\t");
        IntToStr (&l, DepthPlay);
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "Analysis\t");
        IntToStr (&l, Analysis);
        StrCat (&l, '\t');
        IntToStr (&l, AnalysisScorePiece);
        StrCat (&l, '\t');
        IntToStr (&l, AnalysisScoreMove);
        StrCat (&l, '\t');
        IntToStr (&l, AnalysisScoreAttack);
        StrCat (&l, '\t');
        IntToStr (&l, AnalysisScoreAttackInd);
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "Randomize\t");
        IntToStr (&l, Randomize);
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "Font\t");
        StrCat (&l, FontPieces);
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "FontMap\t");
        IntToStr (&l, FontPiecesMap);
        *l = 0;
        FileWriteLine_ (f, Line);
        // Colours
        l = Line;
        StrCat (&l, "Colours\t");
        NumToHex (&l, fMain->cBoard->Colours [0]); StrCat (&l, ',');
        NumToHex (&l, fMain->cBoard->Colours [1]); StrCat (&l, ',');
        NumToHex (&l, fMain->ColourBG [0]); StrCat (&l, ',');
        NumToHex (&l, fMain->ColourBG [1]);
        *l = 0;
        FileWriteLine_ (f, Line);
        // Done Close file & exit
        FileClose (f);
      }
    free (Line);
    return f >= 0;
  }

int Colours [4];

bool SettingsLoad (void)
  {
    int f;   // file ID
    int Size;
    char *Data, *dp, *dp_;
    _DirItem *d;
    int i;
    //
    for (i = 0; i < SIZEARRAY (Colours); i++)
      Colours [i] = -1;
    f = FileOpen_ (FileSettings, foRead);
    if (f >= 0)
      {
        Size = FileSize (f);
        Data = (char *) malloc (Size + 1);
        Data [Size] = 0;   // Terminate String
        FileRead (f, (byte *) Data, Size);
        dp = Data;
        while (*dp)
          {
            // Break off a line
            dp_ = StrPos (dp, '\n');
            *dp_++ = 0;
            // Look for a Data Name
            if (StrMatch (&dp, "Depth\t"))
              DepthPlay = StrGetNum (&dp);
            else if (StrMatch (&dp, "Analysis\t"))
              {
                if (*dp)
                  Analysis = (_Analysis) StrGetNum (&dp);
                if (*dp)
                  AnalysisScorePiece = StrGetNum (&dp);
                if (*dp)
                  AnalysisScoreMove = StrGetNum (&dp);
                if (*dp)
                  AnalysisScoreAttack = StrGetNum (&dp);
                if (*dp)
                  AnalysisScoreAttackInd = StrGetNum (&dp);
              }
            else if (StrMatch (&dp, "Randomize\t"))
              Randomize = StrGetNum (&dp);
            else if (StrMatch (&dp, "Font\t"))
              {
                // find font in Fonts
                d = Fonts;
                FontPiecesSelect = 0;
                while (d)
                  if (StrSame (d->Name, dp))   // Font found
                    {
                      FontPieces = d->Name;
                      d = NULL;
                    }
                  else
                    {
                      d = d->Next;
                      FontPiecesSelect++;
                    }
              }
            else if (StrMatch (&dp, "FontMap\t"))
              FontPiecesMap = StrGetNum (&dp);
            else if (StrMatch (&dp, "Colours\t"))
              {
                for (i = 0; i < SIZEARRAY (Colours); i++)
                  {
                    Colours [i] = StrGetHex (&dp);
                    if (dp)
                      dp++;
                  }
              }
            else
              DebugAddS ("Properties File: Bad Item:", dp);
            dp = dp_;
          }
        FileClose (f);
        free (Data);
        return true;
      }
    return false;
  }

void GraveyardUpdate (void)
  {
    int c;
    _Piece *p;
    char St [256], *s;
    //
    s = St;
    for (c = 0; c <= 1; c++)   // Go through both "sides" of the Graveyard
      {
        p = fMain->Graveyard [c];
        while (*p != pEmpty)
          PieceToChar (&s, *p++);
        if (!c)
          StrCat (&s, '\n');
      }
    *s = 0;
    if (fMain->lGraveyard->Font)
      fMain->lGraveyard->FontSet (FontPieces, fMain->lGraveyard->Font->Size);
    fMain->lGraveyard->TextSet (St);
  }

void GraveyardAddPiece (_Piece Piece)
  {
    _Piece *p;
    //
    p = fMain->Graveyard [PieceWhite (Piece)];   // Appropriate Graveyard "side"
    while (*p != pEmpty)
      p++;
    *p++ = Piece;
    *p = pEmpty;
    Refresh = true;
  }

void GraveyardRemovePiece (_Piece Piece)
  {
    _Piece *p, *q;
    //
    p = fMain->Graveyard [PieceWhite (Piece)];   // Select Grave "side"
    q = NULL;
    while (*p != pEmpty)   // Find end
      {
        q = p;   // note last item (this will be deleted)
        p++;
      }
    if (q)   // Is there an item here
      *q = pEmpty;   // delete it
    Refresh = true;
  }

void MovePiece_ (_Coord From, _Coord To)
  {
    _UndoItem *ui;
    //
    ui = &UndoStack [UndoStackSize];
    ui->From = From;
    ui->To = To;
    ui->OldFrom = Board [From.x][From.y];
    ui->OldTo = Board [To.x][To.y];
    ui->Log = fMain->sLogs;
    //
    if ((MoveID & 1) == 0)   // new White move
      {
        //StrCat (&fMain->sLogs, "\a00\a11\a22\a33\a44\a55\a66\a77\a88\a99 ");
        StrCat (&fMain->sLogs, "\a8");
        NumToStr (&fMain->sLogs, (MoveID >> 1) + 1);
        StrCat (&fMain->sLogs, ".  \a7");
      }
    else   // new black move
      StrCat (&fMain->sLogs, "\a0");
    /*// Set colour
    if (PieceWhite (ui->OldFrom) == PlayerWhite)   // Player's move
      StrCat (&fMain->sLogs, "\a7");   // white
    else
      StrCat (&fMain->sLogs, "\a0");   // black*/
    // Log move and update Graveyard
    CoordToStr (&fMain->sLogs, From);
    if (Piece (ui->OldTo) != pEmpty)   // taking piece
      {
        StrCat (&fMain->sLogs, "\abx\ab");   // show an x
        GraveyardAddPiece (ui->OldTo);   // Add to Graveyard
      }
    else
      StrCat (&fMain->sLogs, ' ');
    CoordToStr (&fMain->sLogs, To);
    ui->SpecMov = MovePiece (From, To);
    if (ui->SpecMov != smNone)
      StrCat (&fMain->sLogs, '*');
    //else
    //  StrCat (&fMain->sLogs, ' ');
    if (ui->SpecMov == smEnPassant)
      GraveyardAddPiece (PieceFrom (pPawn, !PieceWhite (ui->OldFrom)));
    if (MoveID & 1)   // make ready for a Black Move
      StrCat (&fMain->sLogs, '\v');
    else
      StrCat (&fMain->sLogs, '\n');
    //
    if (UndoStackSize  + 1 < SIZEARRAY (UndoStack))   // just in case
      UndoStackSize++;
  }

bool UnmovePiece_ (void)
  {
    _UndoItem *ui;
    //
    if (UndoStackSize == 0)
      return false;
    UndoStackSize--;
    ui = &UndoStack [UndoStackSize];
    UnmovePiece (ui->From, ui->To, ui->OldFrom, ui->OldTo, ui->SpecMov);
    if (Piece (ui->OldTo) != pEmpty)   // replacing taken piece
      GraveyardRemovePiece (ui->OldTo);
    if (ui->SpecMov == smEnPassant)
      GraveyardRemovePiece (PieceFrom (pPawn, !PieceWhite (ui->OldFrom)));
    fMain->sLogs = ui->Log;
    return true;
  }

bool PrevPlayer (void)
  {
    _UndoItem *ui;
    //
    ui = &UndoStack [UndoStackSize];
    return PieceWhite (ui->OldFrom);
  }

bool MoveOposite (_UndoItem *m1, _UndoItem *m2)
  {
    if (m1->From.x == m2->To.x && m1->From.y == m2->To.y)
      if (m1->To.x == m2->From.x && m1->To.y == m2->From.y)
        return true;
    return false;
  }

bool NoDraws = true;

bool CheckRepeat (void)
  {
    if (NoDraws)
      if (UndoStackSize >= 4)
        if (MoveOposite (&UndoStack [UndoStackSize-1], &UndoStack [UndoStackSize-3]))
          if (MoveOposite (&UndoStack [UndoStackSize-2], &UndoStack [UndoStackSize-4]))
            {
              MoveForbidenFrom = UndoStack [UndoStackSize-4].From;
              MoveForbidenTo = UndoStack [UndoStackSize-4].To;
              return true;
            }
    MoveForbidenFrom = {-1, -1};
    MoveForbidenTo = {-1, -1};
    return false;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PROPERTIES FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormProperties: public _Form
  {
    public:
      _Tabs *tPage;
      _Container *cPageAppearence, *cPageChessEngine;
      // PageAppearence
      _CheckBox *cbRotate;
      _Label *lColour;
      _DropList *dlColour;
      _Button *bColourB0, *bColourB1;
      _Button *bColourBG0, *bColourBG1;
      _Label *lFont;
      _DropList *dlFont;
      _Label *lFontMap;
      _DropList *dlFontMap;
      // PageChessEngine
      _Label *lDepth;
      _ButtonArrow *bDepthUp, *bDepthDown;
      _CheckBox *cbNoDraws;
      _Label *lAnalysis;
      _DropList *dlAnalysis;
      _Label *lScorePiece, *lScoreMove, *lScoreAttack, *lScoreAttackInd;
      _EditNumber *eScorePiece, *eScoreMove, *eScoreAttack, *eScoreAttackInd;
      _Label *lRandomize;
      _Slider *sRandomize;
      _FormProperties (char *Title, _Point Position);
     ~_FormProperties (void);
  };

_FormProperties *fProperties;

_FormProperties::~_FormProperties (void)
  {
    fProperties = NULL;
  }

void StrDepth (char *St)
  {
    StrCat (&St, "Depth ");
    IntToStr (&St, DepthPlay);
    *St = 0;
  }

void ActionPropertiesTab (_Tabs *Tabs)
  {
    fProperties->cPageAppearence->VisibleSet (Tabs->Selected == 0);
    fProperties->cPageChessEngine->VisibleSet (Tabs->Selected == 1);
  }

void ActionDepth (_Container *Container)
  {
    _ButtonArrow *b;
    char St [16];
    //
    b = (_ButtonArrow *) Container;
    // Adjust Depth
    if (!b->Down && !PlayThreadStarted)
      if (b->Direction == dUp)
        DepthPlay = Min (DepthPlay + 1, 9);
      else if (b->Direction == dDown)
        DepthPlay = Max (DepthPlay - 1, 0);
    // update label
    StrDepth (St);
    fProperties->lDepth->TextSet (St);
  }

void ActionAnalysis (_Container *Container)
  {
    Analysis = (_Analysis) fProperties->dlAnalysis->Selected;
    NoDraws = fProperties->cbNoDraws->Down;
  }

void ActionAnalysisScore (_Container *Container)
  {
    AnalysisScorePiece = fProperties->eScorePiece->Value;
    AnalysisScoreMove = fProperties->eScoreMove->Value;
    AnalysisScoreAttack = fProperties->eScoreAttack->Value;
    AnalysisScoreAttackInd = fProperties->eScoreAttackInd->Value;
  }

int RandomizeValues [11] = {0, 1, 5, 10, 25, 100, 200, 1000, 2000, 10000, 100000};
                        // {0, 50, 100, 250, 500, 1000, 2000, 4000, 8000, 20000, 50000, 100000};

void ActionRandomize (_Slider *Slider)
  {
    char St [80], *s;
    //
    Randomize = RandomizeValues [Slider->ValueGet ()];
    s = St;
    StrCat (&s, "Randomize\n");
    NumToStr (&s, Randomize);
    *s = 0;
    ((_FormProperties *) (Slider->Form))->lRandomize->TextSet (St);
    DebugAddInt ("Randomize =", Randomize);
  }

void ActionRotate (_CheckBox *CheckBox)
  {
    fMain->cBoard->RotateAllowed = CheckBox->Down;
    fMain->cBoard->Invalidate (true);
  }

void ActionColour (_Container *Container)
  {
    _DropList *dl;
    int Colour0 [4], Colour1 [4];
    //
    Colour0 [0] = ColourAdjust (cBrown, 140);
    Colour1 [0] = ColourAdjust (cBrown, 180);
    Colour0 [1] = 0x549375;
    Colour1 [1] = 0xCEEAEA;
    Colour0 [2] = ColourAdjust (cOrange, 90);
    Colour1 [2] = ColourAdjust (cOrange, 160);
    Colour0 [3] = 0x00C0FF;
    Colour1 [3] = 0x00FCFF;
    dl = (_DropList *) Container;
    if (dl->Selected >= 0)
      {
        fMain->cBoard->Colours [0] = Colour0 [dl->Selected];
        fMain->cBoard->Colours [1] = Colour1 [dl->Selected];
        fMain->cBoard->Invalidate (true);
        fProperties->bColourB0->Colour = Colour0 [dl->Selected];
        fProperties->bColourB0->Invalidate (true);
        fProperties->bColourB1->Colour = Colour1 [dl->Selected];
        fProperties->bColourB1->Invalidate (true);
      }
    Refresh = true;
  }

void ActionColourSelected (int Result, void *Parameter)
  {
    _Button *bColour;
    //
    if (Parameter)
      {
        bColour = (_Button *) Parameter;
        bColour->Colour = Result;
        bColour->Invalidate (true);
        if (bColour->DataInt == 1)
          fMain->cBoard->Colours [0] = Result;
        else if (bColour->DataInt == 2)
          fMain->cBoard->Colours [1] = Result;
        else if (bColour->DataInt == 3)
          fMain->ColourBG [0] = Result;
        else if (bColour->DataInt == 4)
          fMain->ColourBG [1] = Result;
        if (bColour->DataInt <= 2)
          fMain->cBoard->Invalidate (true);
      }
  }

void ActionColourBG (_Button *Button)
  {
    ColourSelect ("Adjust Colour", Button->Colour, ActionColourSelected, Button);
  }

void ActionFont (_DropList *dl)
  {
    _DirItem *di;
    int i;
    //
    i = 0;
    di = Fonts;
    if (dl->Selected >= 0)
      if (ListGoto (Fonts, dl->Selected, &i, &di))
        {
          FontPiecesSelect = dl->Selected;
          FontPieces = di->Name;
          fMain->cBoard->Invalidate (true);
          GraveyardUpdate ();
        }
  }

void ActionFontMap (_DropList *dl)
  {
    if (dl->Selected >= 0)
      {
        FontPiecesMap = dl->Selected;
        fMain->cBoard->Invalidate (true);
        GraveyardUpdate ();
      }
  }

_FormProperties::_FormProperties (char *Title, _Point Position): _Form (Title, {Position.x, Position.y, 240, 240}, waAlwaysOnTop)
  {
    const int Th = 24;
    const int Bdr = 4;
    const int Ht = 28;
    const int bw = Ht * 2 / 3;
    int x, y;
    int v;
    _DirItem *f;
    char *St, *s;
    //
    St = (char *) malloc (MaxPath);
    Container->FontSet (NULL, 12);
    tPage = new _Tabs (Container, {0, 0, 0, Th}, "Appearence\t\a2Chess Engine\a2", (_Action) ActionPropertiesTab);
    // Page Appearence
    cPageAppearence = new _Container (Container, {0, Th, 0, 0});
    x = y = Bdr;
    cbRotate = new _CheckBox (cPageAppearence, {x, y, -Bdr, Ht},"Rotate Board if playing Black", (_Action) ActionRotate);
    cbRotate->Down = fMain->cBoard->RotateAllowed;
    y += Ht + Bdr;
    lColour = new _Label (cPageAppearence, {x, y, 64, Ht},"Colours");
    x += 64;
    bColourB0 = new _Button (cPageAppearence, {x, y, Ht, Ht}, NULL, (_Action) ActionColourBG);
    bColourB0->Colour = fMain->cBoard->Colours [0]; bColourB0->ColourGrad = -1;
    bColourB0->DataInt = 1;
    x += Ht + Bdr;
    bColourB1 = new _Button (cPageAppearence, {x, y, Ht, Ht}, NULL, (_Action) ActionColourBG);
    bColourB1->Colour = fMain->cBoard->Colours [1]; bColourB1->ColourGrad = -1;
    bColourB1->DataInt = 2;
    x += Ht + Bdr;
    dlColour = new _DropList (cPageAppearence, {x, y, -Bdr, Ht}, "Oak\tTeal\tSunset\tNeon", ActionColour);
    y += Ht + Bdr;
    x = 64 + Bdr;
    bColourBG0 = new _Button (cPageAppearence, {x, y, Ht, Ht}, NULL, (_Action) ActionColourBG);
    bColourBG0->Colour = fMain->Container->Colour; bColourBG0->ColourGrad = -1;
    bColourBG0->DataInt = 3;
    x += Ht + Bdr;
    bColourBG1 = new _Button (cPageAppearence, {x, y, Ht, Ht}, NULL, (_Action) ActionColourBG);
    bColourBG1->Colour = fMain->Container->ColourGrad; bColourBG1->ColourGrad = -1;
    bColourBG1->DataInt = 4;
    x = Bdr;
    y += Ht + Bdr;
    lFont = new _Label (cPageAppearence, {x, y, 64, Ht}, "Font (Pieces)"); x += 64;
    s = St;
    f = Fonts;
    while (true)
      {
        StrCat (&s, f->Name);
        f = f->Next;
        if (f)
          StrCat (&s, '\t');
        else
          break;
      }
    *s = 0;
    dlFont = new _DropList (cPageAppearence, {x, y, -Bdr, Ht}, St, (_Action) ActionFont);
    dlFont->Selected = FontPiecesSelect;
    x = Bdr;
    y += Ht + Bdr;
    lFontMap = new _Label (cPageAppearence, {x, y, 64, Ht}, "Font Map"); x += 64;
    dlFontMap = new _DropList (cPageAppearence, {x, y, -Bdr, Ht}, "Map A\tMap B\tUnicode", (_Action) ActionFontMap);
    dlFontMap->Selected = FontPiecesMap;
    // Page Chess Engine
    cPageChessEngine = new _Container (Container, {0, Th, 0, 0});
    cPageChessEngine->VisibleSet (false);
    x = y = Bdr;
    StrDepth (St);
    lDepth = new _Label (cPageChessEngine, {x, y, 64, Ht}, St, aLeft);
    x += 80;
    bDepthUp = new _ButtonArrow (cPageChessEngine, {x, y, bw, Ht / 2}, NULL, ActionDepth, dUp);
    bDepthDown = new _ButtonArrow (cPageChessEngine, {x, y + Ht / 2, bw, Ht / 2}, NULL, ActionDepth, dDown);
    x += bw + bw;
    cbNoDraws = new _CheckBox (cPageChessEngine, {x, y, 0, Ht}, "No Draws", ActionAnalysis);
    cbNoDraws->Down = NoDraws;
    x = Bdr;  //x += Bdr + Ht;
    y += Ht + Bdr;
    lAnalysis = new _Label (cPageChessEngine, {x, y, 0, Ht}, "Analysis");
    x += 80;
    dlAnalysis = new _DropList (cPageChessEngine, {x, y, -Bdr, Ht}, "Simple\tAdd Moves\tAdd Moves Extended\tAdd Moves Defend", ActionAnalysis);
    dlAnalysis->Selected = Analysis;
    x = Bdr;
    y += Ht + Bdr;
    lScorePiece = new _Label (cPageChessEngine, {x, y, 0, Ht}, "Score / Piece"); x += 80;
    eScorePiece = new _EditNumber (cPageChessEngine, {x, y, 64, Ht}, NULL, 0, 1000, ActionAnalysisScore);
    eScorePiece->Value = AnalysisScorePiece;
    y += Ht;
    x = Bdr;
    lScoreMove = new _Label (cPageChessEngine, {x, y, 0, Ht}, "Score / Move"); x += 80;
    eScoreMove = new _EditNumber (cPageChessEngine, {x, y, 64, Ht}, NULL, 0, 1000, ActionAnalysisScore);
    eScoreMove->Value = AnalysisScoreMove;
    y += Ht;
    x = Bdr;
    lScoreAttack = new _Label (cPageChessEngine, {x, y, 0, Ht}, "Score / Attack"); x += 80;
    eScoreAttack = new _EditNumber (cPageChessEngine, {x, y, 64, Ht}, NULL, 0, 1000, ActionAnalysisScore);
    eScoreAttack->Value = AnalysisScoreAttack;
    x = Bdr;
    y += Ht;
    lScoreAttackInd = new _Label (cPageChessEngine, {x, y, 0, Ht}, "Score / Attack'"); x += 80;
    eScoreAttackInd = new _EditNumber (cPageChessEngine, {x, y, 64, Ht}, NULL, 0, 1000, ActionAnalysisScore);
    eScoreAttackInd->Value = AnalysisScoreAttackInd;
    x = Bdr;
    y += Ht + Bdr;
    lRandomize = new _Label (cPageChessEngine, {x, y, 0, Ht}, NULL); x += 80; //"Randomize"
    sRandomize = new _Slider (cPageChessEngine, {x, y, -Bdr, Ht}, 0, 10, (_Action) ActionRandomize);
    v = 0;
    while (RandomizeValues [v] < Randomize)
      v++;
    sRandomize->ValueSet (v);
    ActionRandomize (sRandomize);
    free (St);
  }

void ActionProperties (_Container *Container)
  {
    if (!fProperties)
      fProperties = new _FormProperties ("Setup", MousePos ());
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

char *Help = "\ab\a1Stewy's Chess\ab\a0\e5\n"
             "\nDrag a piece to move.\n"
             "Castling, Crowning & En-Passant allowed.\n"
             "PC plays to win and avoid a Draw.\n"
             "\n"
             "\auSetup\au:\n"
             "  \auAppearance\au: Change Colours & Piece Fonts etc.\n"
             "  \auChess Engine\au: Change how the PC Plays:\n"
             "    \aiDepth\ai: Number of moves to look ahead.\n"
             "    \aiAnalysis\ai: method used to score the future board:\n"
             "      Pieces only, additionally Score Moves, attacks, defences.\n"
             "    \aiScore / Piece\ai: the value of surviving pieces (relative to a Pawn).\n"
             "    \aiScore / Move\ai: the value of each available move.\n"
             "    \aiScore / Attack\ai: the value of each piece you can attack (\").\n"
             "    \aiScore / Attack'\ai: the value of each piece you guard (\").\n"
             "    \aiRandomize\ai adds a random element.\n"
             "\n"
             "\auUndo\au: Take back moves.\n"
             "\n"
             "\auPlay\au: The PC will decide the next move.\n"
             "\n"
             "\auRestart\au: Start again and choose your colour OR play a friend.\n"
             "\n"
             "\auEdit\au: Move any pieces anywhere.\n"
             "  right-click for a new piece.\n"
             "\n"
             "\auStats\au: PC moves considered, time taken & Projected Board Score, in \aiPawns\ai.\n"
             "\n"
             "Right click anywhere for game save etc.\n"
             "\n"
             "\au\a4github.com/StewartTunbridge\n"
             "\a4www.pimicros.com.au\a0\au";

void ActionUndo (_Container *Container)
  {
    Undo = true;
  }

void ActionPlay (_Container *Container)
  {
    PCPlay = true;
  }

void ActionPlayColour (int Result, void *Parameter)//(_Container *Container, int Selection)
  {
    if (Result >= 0)
      {
        PlayerWhite = true;
        PCPlays = true;
        if (Result == 1)
          PlayerWhite = false;
        else if (Result == 2)   // Human verses Human
          PCPlays = false;
        Restart = true;
      }
  }

void ActionRestart (_Container *Container)
  {
    MessageResponseBox ({200, 85}, "What Colour will the human(s) play?", "\a7\abWhite\ab\t\abBlack\ab\t\ab\a1Both\ab", ActionPlayColour);
  }

void ActionEdit (_Button *Button)
  {
    fMain->mPieces->EnabledSet (Button->Down);
    fMain->bUndo->EnabledSet (!Button->Down);
    fMain->bPlay->EnabledSet (!Button->Down);
    UndoStackSize = 0;
    Analyse = true;
  }

void ActionHelp (_Button *Button)
  {
    MessageBox ({500, 400}, Help, 14);
    fMessageBox->lMessage->AlignVert = aTop;
    fMessageBox->lMessage->Align = aLeft;
    fMessageBox->lMessage->Margin = 4;
    fMessageBox->Container->Colour = ColourAdjust (cBlue, 160);
    fMessageBox->Container->ColourGrad = cWhite;
  }

void ActionQuit (_Container *Container)
  {
    Exit = true;
  }

void ActionMenu (_Container *Container)
  {
    _Menu *Menu;
    //
    Menu = (_Menu *) Container;
    if (!PlayThreadStarted)
      if (Menu->Selected == 0)   // Reload
        Load = true;
      else if (Menu->Selected == 1)   // Save
        Save = true;
      else if (Menu->Selected == 2)   // Save Log
        SaveLog = true;
  }

void ActionPieces (_MenuPopup *mp)
  {
    _ChessBoard *cb;
    int dx, dy;
    _Coord Sel;
    //
    if (mp->Selected >= 0)
      {
        cb = (_ChessBoard *) mp->Parent;
        dx = cb->Rect.Width / 8;
        dy = cb->Rect.Height / 8;
        Sel.x = mp->Mouse.x / dx;
        Sel.y = 7 - mp->Mouse.y / dy;
        cb->TranslateCoord (&Sel);
        if (mp->Selected <= pPawn)
          Board [Sel.x][Sel.y] = PieceFrom (mp->Selected, false);
        else
          Board [Sel.x][Sel.y] = PieceFrom (mp->Selected - pPawn, true);
        cb->Move [0][0] = {-1, -1};
        cb->Move [1][0] = {-1, -1};
        cb->Invalidate (true);
        cb->MoveComplete = true;
      }
  }

const _Point FormSize = {630, 600};

_FormMain::_FormMain (char *Title): _Form (Title, {100, 100, FormSize.x, FormSize.y}, waResizable)
  {
    int x, y;
    int btn = 56;
    // Make the Main Form
    ColourBG [0] = cOrange;
    ColourBG [1] = ColourAdjust (cOrange, 80);
    Logs = (char *) malloc (LogsSize);
    sLogs = Logs;
    Container->FontSet (NULL, 14);//, fsNoGrayscale);Ubuntu-R.ttf
    Container->Bitmap = BitmapLoadResource (Window, "Chess2.bmp");
    y = 0;
    // build Toolbar
    Toolbar = new _Container (Container, {0, y, 0, 45});
    x = 0;
    bProperties = new _Button (Toolbar, {x, 0, btn, 0}, "\e0\nSetup", (_Action) ActionProperties); x += btn;
    bUndo = new _Button (Toolbar, {x, 0, btn, 0}, "\e1\nUndo", (_Action) ActionUndo); x += btn;
    bPlay = new _Button (Toolbar, {x, 0, btn, 0}, "\e2\nPlay", (_Action) ActionPlay); x += btn;
    bRestart = new _Button (Toolbar, {x, 0, btn, 0}, "\e3\nRestart", (_Action) ActionRestart); x += btn;
    bEdit = new _Button (Toolbar, {x, 0, btn, 0}, "\e4\nEdit", (_Action) ActionEdit); x += btn;
    bEdit->Toggle = true;
    bHelp = new _Button (Toolbar, {x, 0, btn, 0}, "\e5\nHelp", (_Action) ActionHelp); x += btn;
    bQuit = new _Button (Toolbar, {x, 0, btn, 0}, "\e6\nQuit", (_Action) ActionQuit); x += btn;
    //x += 8;
    lMessage = new _Label (Toolbar, {x, 0, 0, 0}, NULL, aCenter);//, bLowered);
    lMessage->FontSet (NULL, 22, fsBold);
    lMessage->ColourText = cWhite;
    lMessage->Colour = cRed;
    lMessage->VisibleSet (false);
    y += Toolbar->Rect.Height;
    // Dead Pieces go to the Graveyard
    lGraveyard = new _Label (Container, {0, y, BoardWidth, 60}, NULL, aLeft);
    lGraveyard->FontSet (FontPieces, lGraveyard->Rect.Height / 2 - 1);
    lGraveyard->RectLock |= rlRight;
    y += lGraveyard->Rect.Height;
    //
    x = 0;
    cBoard = new _ChessBoard (Container, {x, y, BoardWidth, - 20});
    cBoard->RectLock |= rlRight + rlBottom;
    char s [128];
    AllPieces (s);
    mPieces = new _MenuPopup (cBoard, {}, s, (_Action) ActionPieces);
    mPieces->FontSet (FontPieces, 18);
    mPieces->EnabledSet (false);
    x += BoardWidth;
    //
    lLogs = new _Label (Container, {x + 4, Toolbar->Rect.Height, -8, -20}, NULL, aLeft, bNone);
    lLogs->RectLock = rlLeft | rlBottom;
    lLogs->AlignVert = aBottom;
    lLogs->FontSet (NULL, 14);//, fsBold);
    y += cBoard->Rect.Height;
    //
    lPCStats = new _Label (Container, {0, y, 0, 0}, NULL, aLeft);
    lPCStats->RectLock |= rlTop;
    //
    Wait = new _Wait (lLogs, {0, 0, 0, 0});
    Wait->VisibleSet (false);
    //
    Menu = new _MenuPopup (Container, {0, 0, 0, 0}, "\e7Reload\t\e8Save\t\e8Save Log", ActionMenu);
  }

_FormMain::~_FormMain ()
  {
    free (Logs);
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN LOOP
//
////////////////////////////////////////////////////////////////////////////////////////////////////

int main_ (int argc, char *argv [])
  {
    char St [200], *s;
    _Bitmap *Icon;
    int Col;
    //
    DebugAddS ("===========Start Chess", Revision);
    ResourcePathSet (argv [0]);
    chdir (ResourcePath);
    //GetCurrentPath (&ResourcePath);
    DebugAddS ("Resource Path: ", ResourcePath);
    BoardInit ();
    PlayerWhite = true;
    Restart = true;
    PCPlays = true;
    PCPlay = Analyse = false;
    UndoStackSize = 0;
    s = St;   // Build the form Title
    StrCat (&s, "Stewy's Chess Programme - ");
    StrCat (&s, Revision);
    *s = 0;
    if (FontsRead () == 0)
      DebugAdd ("**** NO CHESS FONTS");
    fMain = new _FormMain (St);
    Icon = BitmapLoad (fMain->Window, "Icon.bmp");
    WindowSetIcon (fMain->Window, Icon);
    BitmapDestroy (Icon);
    SettingsLoad ();
    if (Colours [0] >= 0)
      fMain->cBoard->Colours [0] = Colours [0];
    if (Colours [1] >= 0)
      fMain->cBoard->Colours [1] = Colours [1];
    if (Colours [2] >= 0)
      fMain->ColourBG [0] = Colours [2];
    if (Colours [3] >= 0)
      fMain->ColourBG [1] = Colours [3];
    // Main Loop
    while (!Exit)
      {
        usleep (1000);
        //if (PlayThreadStarted)
        //  cBoard->Invalidate (true);
        fMain->Container->EnabledSet (!FileSelectActive);
        fMain->bUndo->EnabledSet (UndoStackSize > 0);
        if (fMain->sLogs - fMain->Logs + 50 > fMain->LogsSize)   // the problem of c strings is ...
          fMain->sLogs = fMain->Logs;
        if (Restart)
          {
            Restart = false;
            BoardInit ();
            //BoardScoreWhite = 0;
            fMain->cBoard->Move [0][0].x = -1;
            fMain->cBoard->Move [1][0].x = -1;
            fMain->Graveyard [0][0] = pEmpty;
            fMain->Graveyard [1][0] = pEmpty;
            GraveyardUpdate ();
            fMain->sLogs = fMain->Logs;
            fMain->lMessage->VisibleSet (false);
            fMain->lMessage->TextSet (NULL);
            fMain->lPCStats->TextSet (NULL);
            Refresh = true;
            UndoStackSize = 0;
            if (!PlayerWhite)
              if (PCPlays)
                PCPlay = true;
          }
        else if (Load)
          {
            Load = false;
            FileSelect ("Load Game", ".chess", false, GameLoad);
          }
        else if (Save)
          {
            Save = false;
            FileSelect ("Save Game", ".chess", true, GameSave);
          }
        else if (SaveLog)
          {
            SaveLog = false;
            FileSelect ("Save Log", ".log", true, LogSave);
          }
        else if (Undo)
          {
            Undo = false;
            if (UndoStackSize)
              UnmovePiece_();   // so undo a move
            fMain->cBoard->Move [0][0].x = -1;
            fMain->cBoard->Move [1][0].x = -1;
            fMain->lMessage->VisibleSet (false);
            fMain->lMessage->TextSet (NULL);
            if (MoveID == 0)   // starting again
              Restart = true;
            Refresh = true;
          }
        else if (PlayThreadFinished)
          {
            PlayThreadFinished = false;
            fMain->Toolbar->EnabledSet (true);
            // Build / show stats message
            s = St;
            if (PlayThreadWhite)
              StrCat (&s, "\a7");
            else
              StrCat (&s, "\a0");
            StrCat (&s, "Move Stats: ");
            IntToStr (&s, MovesConsidered, DigitsCommas);
            StrCat (&s, " moves considered in ");
            IntToStrDecimals (&s, PlayThreadTime, 3);
            StrCat (&s, " sec.  Score ");
            IntToStrDecimals (&s, BoardScore (PlayThreadWhite), 3);
            StrCat (&s, ", ");
            IntToStrDecimals (&s, PlayThreadScore, 3);
            *s = 0;
            fMain->lPCStats->TextSet (St);
            // Process move
            if (PlayThreadScore == MININT)
              {
                fMain->lMessage->TextSet ("No Moves, It\'s Over");
                fMain->lMessage->VisibleSet (true);
                Beep ();
                PCPlayForever = false;
              }
            else
              {
                MovePiece_ (BestA [0], BestB [0]);   // Update Board [][] ...
                fMain->cBoard->Move [PlayThreadWhite][0] = BestA [0];
                fMain->cBoard->Move [PlayThreadWhite][1] = BestB [0];
                if (InCheck (!PlayThreadWhite))
                  {
                    fMain->lMessage->TextSet ("IN CHECK");
                    fMain->lMessage->VisibleSet (true);
                    Beep ();
                  }
                else
                  fMain->lMessage->VisibleSet (false);
                Refresh = true;
              }
            fMain->Wait->VisibleSet (false);
          }
        else if (PCPlay && !PlayThreadStarted)
          {
            PCPlay = false;
            bool Player = (MoveID & 1) ^ 1;   // Play for whoever's turn it is
            fMain->Toolbar->EnabledSet (false);
            InCheck (Player);   // Mark King if in check
            if (CheckRepeat ())
              fMain->lPCStats->TextSet ("\ab\a1Avoiding Loop\ab");
            StartThread (PlayThread, (void *) Player);
            fMain->Wait->ColourText = Player ? cWhite : cBlack;
            fMain->Wait->VisibleSet (true);
          }
        else if (Analyse)
          {
            Analyse = false;
            s = St;
            StrCat (&s, "White Board Score ");
            IntToStrDecimals (&s, BoardScore (true), 3);
            *s = 0;
            fMain->lPCStats->TextSet (St);
          }
        else if (fMain->cBoard->MoveComplete)
          {
            fMain->cBoard->MoveStart = false;
            fMain->cBoard->MoveComplete = false;
            fMain->lMessage->VisibleSet (false);
            if (fMain->bEdit->Down)   // Editing
              {
                for (Col = 0; Col < 2; Col++)
                  if (fMain->cBoard->Move [Col][0].x >= 0)
                    {
                      Board [fMain->cBoard->Move [Col][1].x][fMain->cBoard->Move [Col][1].y] = Board [fMain->cBoard->Move [Col][0].x][fMain->cBoard->Move [Col][0].y];
                      Board [fMain->cBoard->Move [Col][0].x][fMain->cBoard->Move [Col][0].y] = pEmpty;
                      Analyse = true;
                    }
              }
            else
              {
                if (fMain->cBoard->MoveWhite == (MoveID & 1))  // if ((MoveID & 1) == PlayerWhite)
                  {
                    fMain->lMessage->TextSet ("Not that Colour\'s turn");
                    fMain->lMessage->VisibleSet (true);
                    Beep ();
                    fMain->cBoard->Move [fMain->cBoard->MoveWhite][0].x = -1;
                  }
                else
                  {
                    if (MoveValid (fMain->cBoard->Move [fMain->cBoard->MoveWhite][0], fMain->cBoard->Move [fMain->cBoard->MoveWhite][1]))
                      {
                        MovePiece_ (fMain->cBoard->Move [fMain->cBoard->MoveWhite][0], fMain->cBoard->Move [fMain->cBoard->MoveWhite][1]);
                        if (InCheck (MoveID & 1))   // You moved into / stayed in Check PlayerWhite
                          {
                            UnmovePiece_ ();   // undo move
                            fMain->lMessage->TextSet ("Save The King");
                            fMain->lMessage->VisibleSet (true);
                            Beep ();
                            PCPlayForever = false;
                          }
                        else if ((MoveID & 1) == PlayerWhite)   // Computer's turn
                          if (PCPlays)
                            PCPlay = true;
                      }
                    else
                      {
                        fMain->lMessage->TextSet ("Not a move");
                        fMain->lMessage->VisibleSet (true);
                        Beep ();
                        fMain->cBoard->Move [fMain->cBoard->MoveWhite][0].x = -1;
                        PCPlayForever = false;
                      }
                  }
              }
            Refresh = true;
          }
        else
          if (PCPlayForever && !PlayThreadStarted)
            PCPlay = true;
        if (fMain->ColourBG [0] != fMain->Container->Colour || fMain->ColourBG [1] != fMain->Container->ColourGrad)
          {
            fMain->Container->Colour = fMain->ColourBG [0];
            fMain->Container->ColourGrad = fMain->ColourBG [1];
            fMain->Toolbar->Colour = fMain->ColourBG [0];
            fMain->Toolbar->ColourGrad = fMain->ColourBG [1];
            fMain->lPCStats->Colour = fMain->ColourBG [0];
            fMain->lPCStats->ColourGrad = fMain->ColourBG [1];
            fMain->Container->InvalidateAll (true);
          }
        if (Refresh)
          {
            Refresh = false;
            fMain->cBoard->Invalidate (true);
            *fMain->sLogs = 0;
            fMain->lLogs->TextSet (fMain->Logs);
            GraveyardUpdate ();
          }
        if (FormsUpdate ())
          break;
      }
    SettingsSave ();   // and save settings there
    while (FormList)
      delete (FormList);
    DirFree (Fonts);
    free (ResourcePath);
    return 0;
  }

