///////////////////////////////////////////////////////////////////////////////////////////////////
//
// STEWARTS CHESS PROGRAMME
//
// Author: Stewart Tunbridge, Pi Micros
// Email:  stewarttunbridge@gmail.com
// Copyright (c) 2024 Stewart Tunbridge, Pi Micros
//
///////////////////////////////////////////////////////////////////////////////////////////////////


const char Revision [] = "1.52";

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../Widgets/Widgets.hpp"
#include "../Widgets/WidgetsGrid.hpp"
#include "../Widgets/FileSelect.hpp"
#include "../Widgets/ColourSelect.hpp"
#include "../Widgets/MessageBox.hpp"

#include "../Widgets/libini.hpp"

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

int MoveCount;
bool Refresh;
bool Exit;
bool Load;
bool Save;
bool SaveLog;

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
      UTF8FromInt (Dest, WhitePieceToUChar [FontPiecesMap][Piece (Pce)]);
    else
      UTF8FromInt (Dest, BlackPieceToUChar [FontPiecesMap][Piece (Pce)]);
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
    DirRead (&Fonts, FontsCallBack);
    Fonts = ListSort (Fonts, DirItemsCompare);
    if (Fonts)
      FontPieces = Fonts->Name;
    return ListLength (Fonts);
  }

class _ChessBoard: public _Container
  {
    public:
      int Colours [2];
      bool MoveStart, MoveComplete;
      _Coord Move [2];   // Human move
      _Coord MovePC [2];
      bool LegalMovesShow;
      bool LegalMoves [8][8];
      //
      _ChessBoard (_Container *Parent, _Rect Rect);
      ~_ChessBoard ();
      void DrawCustom (void);
      bool ProcessEventCustom (_Event *Event, _Point Offset);
  };

_ChessBoard::_ChessBoard (_Container *Parent, _Rect Rect) : _Container (Parent, Rect)
  {
    Colour = cBlack;
    Colours [0] = ColourAdjust (cBrown, 140);
    Colours [1] = ColourAdjust (cBrown, 180);
    MoveStart = MoveComplete = false;
    Move [0] = Move [1] = {-1, -1};
    MovePC [0] = MovePC [1] = {-1, -1};
    LegalMovesShow = true;
    memset (LegalMoves, false, sizeof (LegalMoves));
  }

_ChessBoard::~_ChessBoard ()
  {
  }

const int cGreenGray = 0x9cf09c;
//const int cBlueGray = 0xff9c9c;
const int cBlueGray = 0xffaaaa;

#define CenterX(bx) (bx * dx + dx/2)
#define CenterY(by) ((7-by) * dy + dy/2)

void _ChessBoard::DrawCustom ()
  {
    int dx, dy;
    _Rect Square;
    _Coord Center;
    int x, y, Pass;
    int c;
    char p_ [8], *p__;
    _Font *FontTag;
    int cFG, cBG;
    int Width;
    //int id;
    //int t;//####
    //
    //t = ClockMS ();
    Time1 = Time2 = Time3 = 0;
    dx = Rect.Width / 8;
    dy = Rect.Height / 8;
    Width = Max (1, dx / 16);
    FontTag = Parent->FontFind ();   // Use the Forms "normal" font for Tags
    if (FontSet (FontPieces, dy * 4 / 5))   // fonts4free.net/chess-font.html
      for (Pass = 0; Pass < 2; Pass++)
        {
          for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
              {
                Square = {x * dx, (7 - y) * dy, dx, dy};
                Center = {Square.x + dx / 2, Square.y + dy / 2};
                c = Colours [(x ^ y) & 1];
                if (MoveStart)
                  if ((x == Move [0].x && y == Move [0].y) || (x == Move [1].x && y == Move [1].y))
                    c = cWhite;
                if (Pass == 0)
                  DrawRectangle (Square, cBlack, cBlack, c);
                else   // Pass == 1
                  {
                    Square.x++;
                    Square.Width--;
                    if (dy > 40 && FontTag)
                      {
                        cBG = FontTag->ColourBG;
                        cFG = FontTag->Colour;
                        FontTag->ColourBG = c;
                        FontTag->Colour = ColourAdjust (cBlue, 70);
                        if (x == 0)
                          {
                            p_ [0] = y + '1';
                            TextOutAligned (FontTag, Square, p_, aLeft, aCenter);
                          }
                        if (y == 0)
                          {
                            p_ [0] = x + 'a';
                            TextOutAligned (FontTag, Square, p_, aCenter, aRight);
                          }
                        /*/ Debug
                        int id = Board [x][y] / pMoveID;
                        if (id)
                          {
                            char s [16], *ss = s;
                            //NumToHex (&ss, Board [x][y]);
                            NumToStr (&ss, id);
                            *ss = 0;
                            TextOutAligned (FontTag, Square, s, aRight, aTop);
                          }*/
                        FontTag->ColourBG = cBG;
                        FontTag->Colour = cFG;
                      }
                    Font->ColourBG = c;
                    p__ = p_;
                    PieceToChar (&p__, Board [x][y]);
                    *p__ = 0;
                    TextOutAligned (Font, Square, p_, aCenter, aCenter);
                    if (Board [x][y] & pChecked)
                      DrawCircle (Center.x, Center.y, Max (dx / 16, 1), cRed, ColourAdjust (cRed, 150), 1);
                    if (LegalMovesShow && LegalMoves [x][y])
                      {
                        DrawLine (Center.x - dx / 8, Center.y - dy / 8, Center.x + dx / 8, Center.y + dy / 8, cWhite, Width);
                        DrawLine (Center.x + dx / 8, Center.y - dy / 8, Center.x - dx / 8, Center.y + dy / 8, cWhite, Width);
                      }
                  }
              }
          if (Pass == 0)   // Draw User and PC move "arrows"
            {
              if (MovePC [0].x >= 0)
                {
                  DrawLine (MovePC [0].x * dx + dx / 2, (7 - MovePC [0].y) * dy + dy / 2, MovePC [1].x * dx + dx / 2, (7 - MovePC [1].y) * dy + dy / 2, cGreenGray, Width);
                  DrawCircle (CenterX (MovePC [0].x), CenterY (MovePC [0].y), 2 * Width, -1, cGreenGray, 0);
                }
              if (Move [0].x >= 0)
                {
                  DrawLine (Move [0].x * dx + dx / 2, (7 - Move [0].y) * dy + dy / 2, Move [1].x * dx + dx / 2, (7 - Move [1].y) * dy + dy / 2, cBlueGray, Width);
                  DrawCircle (CenterX (Move [0].x), CenterY (Move [0].y), 2 * Width, -1, cBlueGray, 0);
                }
            }
        }
    //DebugAdd ("Draw Board ", ClockMS () - t);
    //DebugAdd ("Draw Board RenderBitmap time (uS) ", Time1); //####
    //DebugAdd ("Draw Board RenderBitmap make trans masks time (uS) ", Time2); //####
    //DebugAdd ("Draw Board RenderBitmap put up img time (uS) ", Time3); //####
  }

bool _ChessBoard::ProcessEventCustom (_Event *Event, _Point Offset)
  {
    int dx, dy;
    _Coord Sel, Moves [64], *m;
    bool Res;
    //
    Res = false;
    if (IsEventMine (Event, Offset) && !PlayThreadStarted)
      {
        dx = Rect.Width / 8;
        dy = Rect.Height / 8;
        Sel.x = (Event->X - Offset.x) / dx;
        Sel.y = 7 - ((Event->Y - Offset.y) / dy);
        if (Event->Type == etMouseDown && Event->Key == KeyMouseLeft)
          {
            Move [0] = Sel;
            Move [1] = Sel;
            // Store possible moves in MovePos []
            memset (LegalMoves, false, sizeof (LegalMoves));
            if (Piece (Board [Sel.x][Sel.y]) != pEmpty)
              {
                GetPieceMoves (Sel, Moves);
                m = Moves;
                while (m->x >= 0)   // for all moves
                  {
                    LegalMoves [m->x][m->y] = true;
                    m++;
                  }
              }
            MoveStart = true;
            Invalidate (true);
            Res = true;
          }
        else if (Event->Type == etMouseMove && MoveStart)
          {
            if (Sel.x != Move [1].x || Sel.y != Move [1].y)
              {
                Move [1] = Sel;
                Invalidate (true);
              }
            Res = true;
          }
        else if (Event->Type == etMouseUp && Event->Key == KeyMouseLeft)
          if (MoveStart)
            {
              if (Move [0].x == Move [1].x && Move [0].y == Move [1].y)   // we haven't moved
                MoveStart = false;
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

const _Point FormSize = {650, 600};

class _FormMain: public _Form
  {
    public:
      const int LogsSize = 2000;
      const int BoardWidth = 500;
      char *Logs, *sLogs;
      _Piece Graveyard [2][64];
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

bool Undo;
bool PlayForMe;
bool Restart;

typedef struct
  {
    _Coord From;
    _Coord To;
    _Piece OldFrom;
    _Piece OldTo;
    _SpecialMove SpecMov;
    char *Log;
  } _UndoItem;

_UndoItem UndoStack [500];

int UndoStackSize;

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
    if ((MoveCount & 1) == 0)   // New Move
      {
        NumToStr (&fMain->sLogs, (MoveCount >> 1) + 1);
        StrCat (&fMain->sLogs, ")  ");
      }
    // Set colour
    if (PieceWhite (ui->OldFrom) == PlayerWhite)   // Player's move
      StrCat (&fMain->sLogs, "\a4");   // blue
    else
      StrCat (&fMain->sLogs, "\a2");   // green
    // Write move
    CoordToStr (&fMain->sLogs, From);
    CoordToStr (&fMain->sLogs, To);
    if (ui->SpecMov != smNone)
      StrCat (&fMain->sLogs, '+');
    if (Piece (ui->OldTo) != pEmpty || ui->SpecMov == smCrown)
      StrCat (&fMain->sLogs, "**");
    // Update Graveyard
    if (Piece (ui->OldTo) != pEmpty)   // taking piece
      GraveyardAddPiece (ui->OldTo);
    ui->SpecMov = MovePiece (From, To);
    if (ui->SpecMov == smEnPassant)
      GraveyardAddPiece (PieceFrom (pPawn, !PieceWhite (ui->OldFrom)));
    // restore colour
    if (PieceWhite (ui->OldFrom) == PlayerWhite)   // Player's move
      StrCat (&fMain->sLogs, "\a4");   // blue
    else
      StrCat (&fMain->sLogs, "\a2");   // green
    if ((MoveCount & 1) == 0)   // New Move
      StrCat (&fMain->sLogs, ' ');
    else
      StrCat (&fMain->sLogs, '\n');
    MoveCount++;
    //
    if (UndoStackSize  + 1 < SIZEARRAY (UndoStack))
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
    MoveCount--;
    return true;
  }

bool PrevPlayer (void)
  {
    _UndoItem *ui;
    //
    ui = &UndoStack [UndoStackSize];
    return PieceWhite (ui->OldFrom);
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ASK COLOUR FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormAskColour: public _Form
  {
    public:
      _Label *lWhatColour;
      _Button *bWhite;
      _Button *bBlack;
      //_DropList *dlColour;
      //_Button *bGo;
      _FormAskColour (_Point Pos);
  };

void ActionPlayColour (_Button *b)
  {
    PlayerWhite = b->DataInt;
    b->Form->Die ();
    Restart = true;
  }

_FormAskColour::_FormAskColour (_Point Pos): _Form (NULL, {Pos.x, Pos.y, 400, 60}, waAlwaysOnTop)
  {
    Container->FontSet (NULL, 16);
    lWhatColour = new _Label (Container, {4, 0, 200, 0}, "What Colour will you play?");
    bWhite = new _Button (Container, {200, 0, 100, 0}, "\a7\abWhite\ab\a7", (_Action) ActionPlayColour);
    bWhite->DataInt = true;
    bBlack = new _Button (Container, {300, 0, 100, 0}, "\abBlack\ab", (_Action) ActionPlayColour);
    bBlack->DataInt = false;
    //dlColour = new _DropList (Container, {200, 0, 0, 0}, "White\tBlack", (_Action) ActionPlayColour);
  }

void SetPlayerColour (_Point Pos)
  {
    new _FormAskColour (Pos);
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PROPERTIES FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormProperties: public _Form
  {
    public:
      _Label *lDepth;
      _ButtonArrow *bDepthUp, *bDepthDown;
      _CheckBox *cbComplex;
      _Label *lRandomize;
      _Slider *sRandomize;
      _Label *lColour;
      _DropList *dlColour;
      _Button *bColourB0, *bColourB1;
      _Button *bColourBG0, *bColourBG1;
      _Label *lFont;
      _DropList *dlFont;
      _Label *lFontMap;
      _DropList *dlFontMap;
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

void ActionComplex (_Container *Container)
  {
    _CheckBox *b;
    //
    b = (_CheckBox *) Container;
    SimpleAnalysis = !b->Down;
  }

int RandomizeValues [] = {0, 50, 100, 250, 500, 1000, 2000, 4000, 8000, 20000, 50000, 100000};

void ActionRandomize (_Slider *Slider)
  {
    Randomize = RandomizeValues [Slider->ValueGet ()];
    DebugAddInt ("Randomize =", Randomize);
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
        fProperties->bColourB0->Colour = Colour0 [dl->Selected];
        fProperties->bColourB0->Invalidate (true);
        fProperties->bColourB1->Colour = Colour1 [dl->Selected];
        fProperties->bColourB1->Invalidate (true);
      }
    Refresh = true;
  }

_Button *bColour = NULL;

void ActionColourSelected (int Colour)
  {
    if (bColour)
      {
        bColour->Colour = Colour;
        bColour->Invalidate (true);
        if (bColour->DataInt == 1)
          fMain->cBoard->Colours [0] = Colour;
        else if (bColour->DataInt == 2)
          fMain->cBoard->Colours [1] = Colour;
        else if (bColour->DataInt == 3)
          {
            fMain->Container->Colour = Colour;
            fMain->lPCStats->Colour = Colour;
          }
        else if (bColour->DataInt == 4)
          {
            fMain->Container->ColourGrad = Colour;
            fMain->lPCStats->ColourGrad = Colour;
          }
        fMain->Container->InvalidateAll (true);
      }
  }

void ActionColourBG (_Container *Container)
  {
    _Button *b;
    //
    b = (_Button *) Container;
    bColour = b;
    if (!b->Down)
      ColourSelect ("Adjust Colour", b->Colour, ActionColourSelected);
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

_FormProperties::_FormProperties (char *Title, _Point Position): _Form (Title, {Position.x, Position.y, 200, 220}, waAlwaysOnTop)
  {
    int x, y;
    const int Bdr = 4;
    const int Ht = 28;
    int v;
    _DirItem *f;
    char *St, *s;
    //
    St = (char *) malloc (MaxPath);
    Container->FontSet (NULL, 12);
    x = y = Bdr;
    StrDepth (St);
    lDepth = new _Label (Container, {x, y, 64, Ht}, St, aLeft);
    x += 64;
    bDepthUp = new _ButtonArrow (Container, {x, y, Ht, Ht / 2}, NULL, ActionDepth, dUp);
    bDepthDown = new _ButtonArrow (Container, {x, y + Ht / 2, Ht, Ht / 2}, NULL, ActionDepth, dDown);
    x += Bdr + Ht;
    cbComplex = new _CheckBox (Container, {Container->Rect.Width - 80 - Bdr, y, 80, Ht}, "Complex Analysis", ActionComplex);
    cbComplex->Align = aRight;
    cbComplex->Down = !SimpleAnalysis;
    x = Bdr;
    y += Ht + Bdr;
    lRandomize = new _Label (Container, {x, y, 64, Ht}, "Randomize");
    x += 64;
    sRandomize = new _Slider (Container, {x, y, Container->Rect.Width - x - Bdr, Ht}, 0, 10, (_Action) ActionRandomize);
    v = 0;
    while (RandomizeValues [v] < Randomize)
      v++;
    sRandomize->ValueSet (v);
    x = Bdr;
    y += Ht + 4 * Bdr;
    lColour = new _Label (Container, {x, y, 64, Ht},"Colours");
    x += 64;
    bColourB0 = new _Button (Container, {x, y, Ht, Ht}, NULL, ActionColourBG);
    bColourB0->Colour = fMain->cBoard->Colours [0]; bColourB0->ColourGrad = -1;
    bColourB0->DataInt = 1;
    x += Ht + Bdr;
    bColourB1 = new _Button (Container, {x, y, Ht, Ht}, NULL, ActionColourBG);
    bColourB1->Colour = fMain->cBoard->Colours [1]; bColourB1->ColourGrad = -1;
    bColourB1->DataInt = 2;
    x += Ht + Bdr;
    //x += 64 + Bdr;
    dlColour = new _DropList (Container, {x, y, 64, Ht}, "Oak\tTeal\tSunset\tNeon", ActionColour);
    y += Ht + Bdr;
    x = 64 + Bdr;
    bColourBG0 = new _Button (Container, {x, y, Ht, Ht}, NULL, ActionColourBG);
    bColourBG0->Colour = fMain->Container->Colour; bColourBG0->ColourGrad = -1;
    bColourBG0->DataInt = 3;
    x += Ht + Bdr;
    bColourBG1 = new _Button (Container, {x, y, Ht, Ht}, NULL, ActionColourBG);
    bColourBG1->Colour = fMain->Container->ColourGrad; bColourBG1->ColourGrad = -1;
    bColourBG1->DataInt = 4;
    x = Bdr;
    y += Ht + Bdr;
    lFont = new _Label (Container, {x, y, 64, Ht}, "Font (Pieces)"); x += 64;
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
    dlFont = new _DropList (Container, {x, y, 100, Ht}, St, (_Action) ActionFont);
    dlFont->Selected = FontPiecesSelect;
    x = Bdr;
    y += Ht + Bdr;
    lFontMap = new _Label (Container, {x, y, 64, Ht}, "Font Map"); x += 64;
    dlFontMap = new _DropList (Container, {x, y, 100, Ht}, "Map A\tMap B\tUnicode", (_Action) ActionFontMap);
    dlFontMap->Selected = FontPiecesMap;
    free (St);
  }

void ActionProperties (_Container *Container)
  {
    _Button *b;
    //
    b = (_Button *) Container;
    if (!b->Down && fProperties == NULL)
      fProperties = new _FormProperties ("Setup", MousePos ());
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

char *Help = "\ab\a1Stewy's Chess\ab\a1\e5\n"
             "\nDrag a piece to move.\n"
             "Castling, Crowning & En-Passant allowed.\n"
             "\auSetup\au: Change PC Play Level, Colours & Fonts. "
             "Randomize adds a random element.\n"
             "\auUndo\au: Take back moves.\n"
             "\auPlay\au: The PC will move for you.\n"
             "\auRestart\au: Start again.\n"
             "\auEdit\au: Move any pieces anywhere,\n"
             "right-click for a new piece.\n"
             "\auStats\au: PC moves considered, time taken & White Board Score, in \aiPawns\ai.\n"
             "\nRight click anywhere for game save etc.\n\n"
             "\au\a4github.com/StewartTunbridge\a4\n"
             "\a4www.pimicros.com.au\a4\au";

void ActionUndo (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      Undo = true;
  }

void ActionPlay (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      PlayForMe = true;
  }

void ActionRestart (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      Restart = true;
  }

void ActionEdit (_Button *Button)
  {
    fMain->cBoard->LegalMovesShow = !Button->Down;
    fMain->mPieces->EnabledSet (Button->Down);
    fMain->bUndo->EnabledSet (!Button->Down);
    fMain->bPlay->EnabledSet (!Button->Down);
    UndoStackSize = 0;
  }

void ActionHelp (_Button *Button)
  {
    if (!Button->Down)
      {
        MessageBox ({400, 400}, Help, 18);
        fMessageBox->Container->Colour = ColourAdjust (cBlue, 160);
        fMessageBox->Container->ColourGrad = cWhite;
      }
  }

void ActionQuit (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
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
    int dx, dy;
    int x, y;
    //
    if (mp->Selected >= 0)
      {
        dx = mp->Parent->Rect.Width / 8;
        dy = mp->Parent->Rect.Height / 8;
        x = mp->Mouse.x / dx;
        y = 7 - mp->Mouse.y / dy;
        if (mp->Selected <= pPawn)
          Board [x][y] = PieceFrom (mp->Selected, false);
        else
          Board [x][y] = PieceFrom (mp->Selected - pPawn, true);
        fMain->cBoard->Invalidate (true);
      }
  }

_FormMain::_FormMain (char *Title): _Form (Title, {100, 100, FormSize.x, FormSize.y}, waResizable)
  {
    int x, y;
    int btn = 56;
    // Make the Main Form
    Logs = (char *) malloc (LogsSize);
    sLogs = Logs;
    Container->Colour = ColourAdjust (cOrange, 100);
    Container->ColourGrad = ColourAdjust (cOrange, 80);
    Container->FontSet ("Ubuntu-R.ttf", 14);//, fsNoGrayscale);
    Container->Bitmap = BitmapLoad (Window, "Chess2.bmp");
    y = 0;
    // build Toolbar
    Toolbar = new _Container (Container, {0, y, 0, 45});
    Toolbar->Colour = ColourAdjust (cForm1, 80);
    x = 0;
    bProperties = new _Button (Toolbar, {x, 0, btn, 0}, "\e0\nSetup", (_Action) ActionProperties); x += btn;
    bUndo = new _Button (Toolbar, {x, 0, btn, 0}, "\e1\nUndo", (_Action) ActionUndo); x += btn;
    bPlay = new _Button (Toolbar, {x, 0, btn, 0}, "\e2\nPlay", (_Action) ActionPlay); x += btn;
    bRestart = new _Button (Toolbar, {x, 0, btn, 0}, "\e3\nRestart", (_Action) ActionRestart); x += btn;
    bEdit = new _Button (Toolbar, {x, 0, btn, 0}, "\e4\nEdit", (_Action) ActionEdit); x += btn;
    bEdit->Toggle = true;
    bHelp = new _Button (Toolbar, {x, 0, btn, 0}, "\e5\nHelp", (_Action) ActionHelp); x += btn;
    x += 8;
    bQuit = new _Button (Toolbar, {x, 0, btn, 0}, "\e6\nQuit", (_Action) ActionQuit); x += btn;
    y += Toolbar->Rect.Height;
    lMessage = new _Label (Toolbar, {x, 0, 0, 0}, NULL, aCenter);
    lMessage->FontSet (NULL, 24, fsBold);
    lMessage->Colour = cRed;
    lMessage->VisibleSet (false);
    // Dead Pieces go to the Graveyard
    lGraveyard = new _Label (Container, {0, y, BoardWidth, 60}, NULL, aLeft);
    lGraveyard->FontSet (FontPieces, lGraveyard->Rect.Height / 2 - 1);
    lGraveyard->RectLock |= rlRight;
    y += lGraveyard->Rect.Height;
    //
    x = 0;
    cBoard = new _ChessBoard (Container, {x, y, BoardWidth, Container->Rect.Height - y - 20});
    cBoard->RectLock |= rlRight + rlBottom;
    //cBoard->Colour = cGray;
    char s [128];
    AllPieces (s);
    //mPieces = new _MenuPopup (cBoard, {}, "Empty\tKing\tQueen\tRook\tBishop\tKnight\tPawn\t\ebBlack <-> White\eb", (_Action) ActionPieces);
    mPieces = new _MenuPopup (cBoard, {}, s, (_Action) ActionPieces);
    mPieces->FontSet (FontPieces, 18);
    mPieces->EnabledSet (false);
    //y += cBoard->Rect.Height;
    x += BoardWidth;
    //
    lLogs = new _Label (Container, {x + 8, Toolbar->Rect.Height, 0, lGraveyard->Rect.Height + cBoard->Rect.Height - 8}, NULL, aLeft, bNone);
    lLogs->RectLock = rlLeft | rlBottom;
    lLogs->AlignVert = aRight;   // = Bottom
    //lLogs->ColourText = ColourAdjust (cAqua, 20); //ColourAdjust (cOrange, 70); cBrown;
    lLogs->FontSet (NULL, 14, fsBold);
    y += cBoard->Rect.Height;
    //
    lPCStats = new _Label (Container, {0, y, 0, 0}, NULL, aLeft);
    lPCStats->RectLock |= rlTop;
    lPCStats->Colour = Container->Colour;//cYellow;//cOrange;
    lPCStats->ColourGrad = Container->ColourGrad;//cYellow;//cOrange;
    lPCStats->ColourText = ColourAdjust (cGreen, 20);
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

bool FileWriteLine_ (int f, char *Line)
  {
    bool Res;
    //
    StrChangeChar (Line, '\n', ',');
    Res = FileWriteLine (f, Line);
    StrChangeChar (Line, ',', '\n');
    return Res;
  }

void GameSave (char *Name)
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
        NumToStr (&l, (MoveCount >> 1) + 1);
        *l = 0;
        FileWriteLine_ (f, Line);
        FileClose (f);
      }
    else
      StrCat (&fMain->sLogs, "*** ERROR SAVING FILE ***\n");
    free (Line);
  }

void GameLoad (char *Name)
  {
    int f;   // file ID
    int Size;
    char *Data, *dp, *dp_;
    int x, y;
    _Piece *p;
    //
    f = FileOpen (Name, foRead);
    if (f >= 0)
      {
        fMain->Graveyard [0][0] = fMain->Graveyard [1][0] = pEmpty;
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
            else if (StrMatch (&dp, "Graveyard\t"))
              {
                p = fMain->Graveyard [0];
                while (*dp)
                  {
                    if (*dp == '\t')
                      {
                        p = fMain->Graveyard [1];
                        dp++;
                      }
                    if (*dp)
                      {
                        *p++ = (_Piece) StrGetHex (&dp);
                        *p = pEmpty;
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
              MoveCount = (StrGetNum (&dp) - 1) << 1;
            else
              break;
            dp = dp_;
          }
        FileClose (f);
        free (Data);
        fMain->sLogs = StrPos (fMain->Logs, (char) 0);
        UndoStackSize = 0;
        fMain->cBoard->Move [0].x = -1;
        fMain->cBoard->MovePC [0].x = -1;
        fMain->lMessage->VisibleSet (false);
        //Form->Container->EnabledSet (true);
        Refresh = true;
      }
    else
      StrCat (&fMain->sLogs, "*** ERROR LOADING FILE ***\n");
  }

void LogSave (char *Name)
  {
    int f;   // file ID
    //
    f = FileOpen (Name, foWrite);
    if (f >= 0)
      {
        FileWrite (f, (byte *) fMain->Logs, StrLen (fMain->Logs));
        FileClose (f);
      }
    else
      StrCat (&fMain->sLogs, "*** ERROR SAVING FILE ***\n");
  }

#define FileSettings "Chess.properties"

bool SettingsSave (void)
  {
    int f;   // file ID
    char *Line, *l;
    //
    Line = (char *) malloc (250);
    f = FileOpen (FileSettings, foWrite);
    if (f >= 0)
      {
        l = Line;
        StrCat (&l, "Depth\t");
        IntToStr (&l, DepthPlay);
        *l = 0;
        FileWriteLine_ (f, Line);
        //
        l = Line;
        StrCat (&l, "Simple\t");
        IntToStr (&l, SimpleAnalysis);
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
        NumToHex (&l, fMain->Container->Colour); StrCat (&l, ',');
        NumToHex (&l, fMain->Container->ColourGrad);
        *l = 0;
        FileWriteLine_ (f, Line);
        // Done Close file & exit
        FileClose (f);
        return true;
      }
    return false;
  }

bool SettingsLoad (void)
  {
    int f;   // file ID
    int Size;
    char *Data, *dp, *dp_;
    _DirItem *d;
    //
    f = FileOpen (FileSettings, foRead);
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
            else if (StrMatch (&dp, "Simple\t"))
              SimpleAnalysis = (bool) StrGetNum (&dp);
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
                fMain->cBoard->Colours [0] = StrGetHex (&dp);
                if (dp)  dp++;
                fMain->cBoard->Colours [1] = StrGetHex (&dp);
                if (dp)  dp++;
                fMain->Container->Colour = StrGetHex (&dp);
                fMain->lPCStats->Colour = fMain->Container->Colour;
                if (dp)  dp++;
                fMain->Container->ColourGrad = StrGetHex (&dp);
                fMain->lPCStats->ColourGrad = fMain->Container->ColourGrad;
              }
            else
              DebugAddS ("Properties File Error: ", dp);
            dp = dp_;
          }
        FileClose (f);
        free (Data);
        return true;
      }
    return false;
  }

int main_ (int argc, char *argv [])
  {
    bool PCPlay;
    _Coord MovePC [2];
    char St [200], *s;
    char *Path;
    //
    DebugAddS ("===========Start Chess", Revision);
    if (GetCurrentPath (&Path))
      {
        DebugAddS ("Home Path: ", Path);
        free (Path);
      }
    Path = nullptr;
    if (StrExtractPath (argv [0], &Path))   // change directory to that included in the "exe" path, to make launching easier
      {
        DebugAddS ("Exe Path: ", Path);
        chdir (Path);
      }
    BoardInit ();
    PlayerWhite = true;
    PCPlay = false;
    // Main Loop
    Restart = true;
    PlayForMe = PCPlay = false;
    UndoStackSize = 0;
    s = St;   // Build the form Title
    StrCat (&s, "Stewy's Chess Programme - ");
    StrCat (&s, Revision);
    *s = 0;
    if (FontsRead () == 0)
      DebugAdd ("**** NO CHESS FONTS");
    fMain = new _FormMain (St);
    SettingsLoad ();
    _Point p;
    WindowGetPosSize (fMain->Window, &p.x, &p.y, NULL, NULL);
    SetPlayerColour (p);
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
            MoveCount = 0;
            BoardInit ();
            fMain->cBoard->Move [0].x = -1;
            fMain->cBoard->MovePC [0].x = -1;
            fMain->Graveyard [0][0] = pEmpty;
            fMain->Graveyard [1][0] = pEmpty;
            GraveyardUpdate ();
            fMain->sLogs = fMain->Logs;
            fMain->lMessage->VisibleSet (false);
            fMain->lPCStats->TextSet (NULL);
            Refresh = true;
            UndoStackSize = 0;
            if (!PlayerWhite)
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
            while (true)
              {
                if (UndoStackSize == 0)
                  break;
                UnmovePiece_();   // so undo a move
                if (PrevPlayer () == PlayerWhite)   // until next undo is opponent
                  break;
              }
            fMain->cBoard->Move [0].x = -1;
            fMain->cBoard->MovePC [0].x = -1;
            fMain->lMessage->VisibleSet (false);
            if (MoveCount == 0)   // starting again
              Restart = true;
            Refresh = true;
          }
        else if (PlayThreadFinished)
          {
            PlayThreadFinished = false;
            fMain->Toolbar->EnabledSet (true);
            if (PlayThreadWhite == PlayerWhite)   // playing for the human
              if (PlayThreadScore == MININT)
                {
                  fMain->lMessage->TextSet ("No Moves, Give up");
                  fMain->lMessage->VisibleSet (true);
                }
              else
                {
                  fMain->cBoard->Move [0] = BestA [0];
                  fMain->cBoard->Move [1] = BestB [0];
                  fMain->cBoard->MoveComplete = true;
                }
            else   // playing for the PC
              {
                // Build / show stats message
                s = St;
                StrCat (&s, "Stats: ");
                IntToStr (&s, MovesConsidered, DigitsCommas);
                StrCat (&s, " moves considered in ");
                IntToStrDecimals (&s, PlayThreadTime, 3);
                StrCat (&s, " sec.  Score ");
                IntToStrDecimals (&s, -PlayThreadScore, 3);
                *s = 0;
                fMain->lPCStats->TextSet (St);
                //
                if (PlayThreadScore == MININT)
                  {
                    fMain->lMessage->TextSet ("I Surrender");
                    fMain->lMessage->VisibleSet (true);
                  }
                else
                  {
                    MovePC [0] = BestA [0];   // Extract the actual move
                    MovePC [1] = BestB [0];
                    MovePiece_ (MovePC [0], MovePC [1]);   // Update Board [][] ...
                    fMain->cBoard->MovePC [0] = MovePC [0];   // Update cBoard
                    fMain->cBoard->MovePC [1] = MovePC [1];
                    fMain->cBoard->Invalidate (true);
                    if (InCheck (PlayerWhite))
                      {
                        fMain->lMessage->TextSet ("IN CHECK");
                        fMain->lMessage->VisibleSet (true);
                      }
                    else
                      fMain->lMessage->VisibleSet (false);
                  }
                Refresh = true;
              }
            //cBoard->EnabledSet (true);
            fMain->Wait->VisibleSet (false);
          }
        else if (PCPlay)
          {
            PCPlay = false;
            fMain->Toolbar->EnabledSet (false);
            InCheck (!PlayerWhite);   // Mark PC King if in check
            StartThread (PlayThread, (void *) !PlayerWhite);
            fMain->Wait->ColourText = ColourAdjust (cGreen, 25);
            fMain->Wait->VisibleSet (true);
          }
        else if (PlayForMe)
          {
            PlayForMe = false;
            fMain->Toolbar->EnabledSet (false);
            StartThread (PlayThread, (void *) PlayerWhite);
            fMain->Wait->ColourText = ColourAdjust (cBlue, 50);
            fMain->Wait->VisibleSet (true);
          }
        else if (fMain->cBoard->MoveComplete)
          {
            fMain->cBoard->MoveStart = false;
            fMain->cBoard->MoveComplete = false;
            if (fMain->bEdit->Down)
              {
                Board [fMain->cBoard->Move [1].x][fMain->cBoard->Move [1].y] = Board [fMain->cBoard->Move [0].x][fMain->cBoard->Move [0].y];
                Board [fMain->cBoard->Move [0].x][fMain->cBoard->Move [0].y] = pEmpty;
              }
            else
              {
                if (MoveValid (fMain->cBoard->Move [0], fMain->cBoard->Move [1]))
                  {
                    //s = fMain->sLogs;
                    MovePiece_ (fMain->cBoard->Move [0], fMain->cBoard->Move [1]);
                    if (InCheck (PlayerWhite))   // You moved into / stayed in Check
                      {
                        UnmovePiece_ ();   // undo move
                        //fMain->sLogs = s;
                        fMain->lMessage->TextSet ("Save Your King");
                        fMain->lMessage->VisibleSet (true);
                        //StrCat (&fMain->sLogs, "Error\n");
                      }
                    else
                      PCPlay = true;
                  }
                else
                  {
                    fMain->lMessage->TextSet ("Not a move");
                    fMain->lMessage->VisibleSet (true);
                    //StrCat (&fMain->sLogs, "Error\n");
                    fMain->cBoard->Move [0].x = -1;
                  }
              }
            Refresh = true;
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
    while (FormList)
      delete (FormList);
    chdir (Path);   // return to exe directory
    SettingsSave ();   // and save settings there
    DirFree (Fonts);
    free (Path);   // only now can we free this
    return 0;
  }


/********************************************************************************************************************* JUNK
                Name = (char *) malloc (64);
                sN = Name;
                StrCat (&sN, "board");
                CurrentDateTimeToStr (&sN, true, true);
                StrCat (&sN, ".dat");
                *sN = 0;

    char par [] = "Param # :";
    for (x = 0; x < argc; x++)
      {
        par [6] = x + '0';
        DebugAddS (par, argv [x]);
      }

*********************************************************************************************************************/
