///////////////////////////////////////////////////////////////////////////////////////////////////
//
// STEWARTS CHESS PROGRAMME
//
// Author: Stewart Tunbridge, Pi Micros
// Email:  stewarttunbridge@gmail.com
// Copyright (c) 2024 Stewart Tunbridge, Pi Micros
//
///////////////////////////////////////////////////////////////////////////////////////////////////


const char Revision [] = "0.96";

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../Widgets/Widgets.hpp"

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
    NumToStrDecimals (&ps, t - Time0, 3);
    StrCat (&ps, " [");
    NumToStrDecimals (&ps, t - Time1, 3);
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

char *BlackPieceToChar = " lwtnjo";   // pEmpty, pKing, pQueen, pRook, pBishop, pKnight, pPawn
char *WhitePieceToChar = " kqrbhp";

char PieceToChar (int Pce)
  {
    if (PieceWhite (Pce))
      return WhitePieceToChar [Piece (Pce)];
    return BlackPieceToChar [Piece (Pce)];
  }

int MoveCount;
int BoardColour;
bool Refresh;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BOARD VISUAL COMPONENT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _BoardC: public _Container
  {
    public:
      int Colours [2];
      bool MoveStart, MoveComplete;
      _Coord MoveFrom, MoveTo;   // Human move
      _Coord PCFrom = {-1, -1}, PCTo;   // PC Move
      //
      _BoardC (_Container *Parent, _Rect Rect);
      void DrawCustom (void);
      bool ProcessEventCustom (_Event *Event, _Point Offset);
  };

_BoardC::_BoardC (_Container *Parent, _Rect Rect) : _Container (Parent, Rect)
  {
    Colours [0] = ColourAdjust (cBrown, 140);
    Colours [1] = ColourAdjust (cBrown, 180);
    MoveStart = MoveComplete = false;
  }

const int cGreenGray = 0x9cf09c;
const int cBlueGray = 0xff9c9c;

void _BoardC::DrawCustom ()
  {
    int dx, dy;
    _Rect Square;
    int x, y, Pass;
    int c;
    char p_ [2];
    //
    dx = Rect.Width / 8;
    dy = Rect.Height / 8;
    if (FontSet ("CHEQ_TT.TTF", dy * 4 / 5))   // fonts4free.net/chess-font.html
      for (Pass = 0; Pass < 2; Pass++)
        {
          for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
              {
                Square = {x * dx, (7 - y) * dy, dx, dy};
                c = Colours [(x ^ y) & 1];
                if (MoveStart)
                  if ((x == MoveFrom.x && y == MoveFrom.y) || (x == MoveTo.x && y == MoveTo.y))
                    c = cWhite;
                if (Pass == 0)
                  DrawRectangle (Square, cBlack, cBlack, c);
                else   // Pass == 1
                  {
                    p_ [0] = PieceToChar (Board [x][y]);
                    p_ [1] = 0;
                    Font->ColourBG = c;
                    TextOutAligned (Font, Square, p_, aCenter, aCenter);
                    /* //Debug - testing pieces attributes
                    if (Board [x][y] & pChecked)
                      DrawLine (Square.x, Square.y, Square.x + dx, Square.y + dy, cRed, 1);
                    if ((int) Board [x][y] / pMoves > 0)
                      DrawLine (Square.x + dx, Square.y, Square.x, Square.y + dy, cBlue, 1);
                    */
                  }
              }
          if (Pass == 0)   // Draw PC move "arrow"
            {
              c = Max (1, dx / 16);   // Width
              if (PCFrom.x >= 0)
                DrawLine (PCFrom.x * dx + dx / 2, (7 - PCFrom.y) * dy + dy / 2, PCTo.x * dx + dx / 2, (7 - PCTo.y) * dy + dy / 2, cGreenGray, c);
              if (MoveFrom.x >= 0)
                DrawLine (MoveFrom.x * dx + dx / 2, (7 - MoveFrom.y) * dy + dy / 2, MoveTo.x * dx + dx / 2, (7 - MoveTo.y) * dy + dy / 2, cBlueGray, c);
            }
        }
  }

bool _BoardC::ProcessEventCustom (_Event *Event, _Point Offset)
  {
    int dx, dy;
    _Coord Sel;
    bool Res;
    //
    Res = false;
    if (IsEventMine (Event, Offset))
      {
        dx = Rect.Width / 8;
        dy = Rect.Height / 8;
        Sel.x = (Event->X - Offset.x) / dx;
        Sel.y = 7 - ((Event->Y - Offset.y) / dy);
        if (Event->Type == etMouseDown && Event->Key == KeyMouseLeft)
          {
            MoveFrom = Sel;
            MoveTo = Sel;
            MoveStart = true;
            Invalidate (true);
            Res = true;
          }
        else if (Event->Type == etMouseMove && MoveStart)
          {
            if (Sel.x != MoveTo.x || Sel.y != MoveTo.y)
              {
                MoveTo = Sel;
                Invalidate (true);
              }
            Res = true;
          }
        else if (Event->Type == etMouseUp && Event->Key == KeyMouseLeft)
          {
            if (MoveFrom.x == MoveTo.x && MoveFrom.y == MoveTo.y)   // we haven't moved
              MoveStart = false;
            else
              MoveComplete = true;
            Invalidate (true);
            Res = true;
          }
      }
    return Res;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PROPERTIES FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

_BoardC *cBoard;
_Form *fProperties;
_Button *bProperties;
_Label *lDepth;

class _FormProperties: public _Form
  {
    public:
      _FormProperties (char *Title, _Rect Position):
        _Form (Title, Position, waAlwaysOnTop) //waFullScreen | waBorderless) //
          {
          }
      ~_FormProperties (void);
  };

_FormProperties::~_FormProperties (void)
  {
    fProperties = NULL;
    bProperties->Down = false;
    bProperties->Invalidate (true);
  }

void ActionDepth (_Container *Container)
  {
    _ButtonArrow *b;
    char St [16], *s;
    //
    b = (_ButtonArrow *) Container;
    if (b)
      if (!b->Down)
        if (b->Direction == dUp)
          DepthPlay = Min (DepthPlay + 1, 9);
        else if (b->Direction == dDown)
          DepthPlay = Max (DepthPlay - 1, 0);
    // update label
    if (lDepth)
      {
        s = St;
        StrCat (&s, "Depth ");
        IntToStr (&s, DepthPlay);
        *s = 0;
        lDepth->TextSet (St);
      }
  }

void ActionComplex (_Container *Container)
  {
    _CheckBox *b;
    //
    b = (_CheckBox *) Container;
    SimpleAnalysis = !b->Down;
  }

void ActionColour (_Container *Container)
  {
    _DropList *dl;
    //
    dl = (_DropList *) Container;
    if (dl->Selected >= 0)
      {
        BoardColour = dl->Selected;
        switch (BoardColour)
          {
            case 0: cBoard->Colours [0] = ColourAdjust (cBrown, 140);
                    cBoard->Colours [1] = ColourAdjust (cBrown, 180);
                    break;
            case 1: cBoard->Colours [0] = 0x549375; //ColourAdjust (cAqua, 80);
                    cBoard->Colours [1] = 0xCEEAEA; //ColourAdjust (cAqua, 60);
                    break;
            case 2: cBoard->Colours [0] = ColourAdjust (cOrange, 90);
                    cBoard->Colours [1] = ColourAdjust (cOrange, 160);
                    break;
            case 3: cBoard->Colours [0] = 0x00C0FF;
                    cBoard->Colours [1] = 0x00FCFF;
          }
      }
    Refresh = true;
  }

void ActionProperties (_Container *Container)
  {
    _Button *b;
    _Label *lColour;
    _DropList *dlColour;
    _ButtonArrow *bDepthUp, *bDepthDown;
    _CheckBox *cbComplex;
    int x, y;
    const int bdr = 4;
    const int Ht = 28;
    //
    b = (_Button *) Container;
    if (b->Down)
      {
        fProperties = new _FormProperties ("Setup", {200, 200, 140, 108});
        fProperties->Container->FontSet ("ARI.ttf", 12);
        x = y = bdr;
        lColour = new _Label (fProperties->Container, {x, y, 64, Ht},"Colour");
        x += 64;
        dlColour = new _DropList (fProperties->Container, {x, y, 64, Ht}, "Oak\tTeal\tSunset\tNeon", ActionColour);
        dlColour->Selected = BoardColour;
        x = bdr;
        y += Ht + bdr + bdr;
        lDepth = new _Label (fProperties->Container, {x, y, 64, Ht}, NULL, aLeft);
        ActionDepth (NULL);
        x += 64;
        bDepthUp = new _ButtonArrow (fProperties->Container, {x, y, Ht, Ht / 2}, NULL, ActionDepth, dUp);
        bDepthDown = new _ButtonArrow (fProperties->Container, {x, y + Ht / 2, Ht, Ht / 2}, NULL, ActionDepth, dDown);
        x = bdr;
        y += Ht + bdr;
        cbComplex = new _CheckBox (fProperties->Container, {x, y, 128, Ht}, "Complex Analysis", ActionComplex);
        cbComplex->Down = !SimpleAnalysis;
      }
    else
      {
        delete fProperties;
        fProperties = NULL;
      }
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN FORM and LOOP
//
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Restart;
bool Undo;
bool PlayForMe;

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

void ActionQuit (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      GameOver = true;
  }

void ActionMenu (_Container *Container)
  {
    _Menu *Menu;
    int f;   // file ID
    //
    Menu = (_Menu *) Container;
    if (Menu->Selected == 0)   // Reload
      {
        f = FileOpen ("board.dat", foRead);
        if (f >= 0)
          {
            FileRead (f, (byte *) Board, sizeof (Board));
            FileClose (f);
            Refresh = true;
          }
      }
    else if (Menu->Selected == 1)   // Save
      {
        f = FileOpen ("board.dat", foWrite);
        FileWrite (f, (byte *) Board, sizeof (Board));
        FileClose (f);
      }
  }

bool TakePiece (_Coord Pos, char **stat, char **grave)
  {
    _Piece p;
    //
    p = (_Piece) Board [Pos.x][Pos.y];
    if (Piece (p) != pEmpty)
      {
        StrCat (stat, "**");
        StrCat (grave, PieceToChar (p));
        return true;
      }
    return false;
  }

const int StatusSize = 2000;

int main_ (int argc, char *argv [])
  {
    _Coord FormSize = {650, 600};
    int BoardWidth = 500;
    int btn = 64;
    _Form *Form;
    _Container *Toolbar;
    _Button *bUndo, *bPlay, *bRestart, *bQuit;
    _Label *lMessage;
    _Label *lGraveyard;
    _Label *lPCStats;
    _Label *lLog;
    _MenuPopup *Menu;
    bool PCPlay;
    _Coord PCFrom, PCTo;
    int PlayTime;
    int PlayScore;
    int x, y;
    char *Status, *sS;
    char Graveyard [64], *sG, *sGOld;
    char PCStats [100], *pPC;
    _SpecialMove sm;
    _Board BoardOld;
    bool CanUndo;
    //
    DebugAddS ("===========Start Chess", Revision);
    BoardInit ();
    PlayerWhite = true;
    PCPlay = false;
    Status = (char *) malloc (StatusSize);
    // Make the Main Form
    sS = Status;   // Build the form Title
    StrCat (&sS, "Stewy's Chess Programme - ");
    StrCat (&sS, Revision);
    *sS = 0;
    Form = new _Form (Status, {100, 100, FormSize.x, FormSize.y}, waResizable);
    sS = Status;
    sG = Graveyard;
    Form->Container->FontSet ("ARI.ttf", 14);
    Form->Container->Bitmap = BitmapLoad (Form->Window, "Chess.bmp");
    y = 0;
    // build Toolbar
    Toolbar = new _Container (Form->Container, {0, y, 0, 50});
    Toolbar->Colour = ColourAdjust (cForm1, 80);
    x = 0;
    bProperties = new _Button (Toolbar, {x, 0, btn, 0}, "\e0\nSetup", (_Action) ActionProperties); x += btn;
    bProperties->Toggle = true;
    bUndo = new _Button (Toolbar, {x, 0, btn, 0}, "\e1\nUndo", (_Action) ActionUndo); x += btn;
    bPlay = new _Button (Toolbar, {x, 0, btn, 0}, "\e2\nPlay", (_Action) ActionPlay); x += btn;
    bRestart = new _Button (Toolbar, {x, 0, btn, 0}, "\e3\nRestart", (_Action) ActionRestart); x += btn;
    bQuit = new _Button (Toolbar, {x, 0, btn, 0}, "\e4\nQuit", (_Action) ActionQuit); x += btn;
    y += Toolbar->Rect.Height;
    lMessage = new _Label (Toolbar, {x, 0, 0, 0}, NULL, aCenter);
    lMessage->FontSet (NULL, 35, fsBold);
    lMessage->Colour = cRed;
    lMessage->VisibleSet (false);
    // Dead Pieces go to the Graveyard
    lGraveyard = new _Label (Form->Container, {0, y, 0, 28}, NULL, aLeft);
    lGraveyard->FontSet ("CHEQ_TT.TTF", lGraveyard->Rect.Height);
    y += 28;
    //
    x = 0;
    cBoard = new _BoardC (Form->Container, {x, y, BoardWidth, Form->Container->Rect.Height - y - 20});
    cBoard->RectLock |= rlRight + rlBottom;
    cBoard->Colour = cGray;
    x += BoardWidth;
    //
    lLog = new _Label (Form->Container, {x + 8, y, 0, cBoard->Rect.Height}, NULL, aLeft, bNone);
    lLog->RectLock = rlLeft | rlBottom;
    lLog->AlignVert = aRight;   // = Bottom
    lLog->ColourText = ColourAdjust (cAqua, 40);  // ColourAdjust (cOrange, 70);
    lLog->FontSet ("ARI.ttf", 14, fsBold);
    y += cBoard->Rect.Height;
    //
    lPCStats = new _Label (Form->Container, {0, y, 0, 0}, NULL, aLeft);
    lPCStats->RectLock = rlTop | rlBottom;
    lPCStats->ColourText = ColourAdjust (cGreen, 30);
    //
    Menu = new _MenuPopup (Form->Container, {}, "Reload\tSave", ActionMenu);
    // Main Loop
    Restart = true;
    PlayForMe = PCPlay = CanUndo = false;
    while (!GameOver)
      {
        usleep (1000);
        if (Restart)
          {
            Restart = false;
            MoveCount = 1;
            BoardInit ();
            cBoard->MoveFrom.x = -1;
            cBoard->PCFrom.x = -1;
            sG = Graveyard;
            sS = Status;
            lMessage->VisibleSet (false);
            lPCStats->TextSet (NULL);
            Refresh = true;
            Undo = CanUndo = false;
          }
        else if (sS - Status  + 50 > StatusSize)   // the problem of c strings is ...
          sS = Status;
        if (Undo)
          {
            Undo = false;
            if (CanUndo)
              {
                CanUndo = false;
                memcpy (Board, BoardOld, sizeof (_Board));
                cBoard->MoveFrom.x = -1;
                cBoard->PCFrom.x = -1;
                sG = sGOld;
                MoveCount--;
                Refresh = true;
              }
          }
        if (PCPlay)
          {
            PCPlay = false;
            MovesConsidered = 0;
            PlayTime = ClockMS ();
            InCheck (!PlayerWhite);   // Mark PC King if in check
            PlayScore = BestMove (!PlayerWhite, 0);
            if (PlayScore != MININT)
              {
                pPC = PCStats;
                StrCat (&pPC, "PC Move Stats: ");
                IntToStr (&pPC, MovesConsidered, DigitsCommas);
                StrCat (&pPC, " moves.  Score ");
                IntToStr (&pPC, PlayScore, DigitsCommas);
                StrCat (&pPC, ".  Time ");
                NumToStrDecimals (&pPC, ClockMS () - PlayTime, 3);
                StrCat (&pPC, " s.  ");
                for (x = 0; x < DepthPlay; x++)
                  {
                    CoordToStr (&pPC, BestA_ [x]);
                    StrCat (&pPC, '-');
                    CoordToStr (&pPC, BestB_ [x]);
                    StrCat (&pPC, ' ');
                  }
                *pPC = 0;
                lPCStats->TextSet (PCStats);
                PCFrom = BestA [0];
                PCTo = BestB [0];
                CoordToStr (&sS, PCFrom);
                CoordToStr (&sS, PCTo);
                TakePiece (PCTo, &sS, &sG);
                sm = MovePiece (PCFrom, PCTo);
                MoveCount++;
                cBoard->PCFrom = PCFrom;
                cBoard->PCTo = PCTo;
                cBoard->Invalidate (true);
                if (sm != smNone)
                  StrCat (&sS, '+');
                StrCat (&sS, '\n');
                Refresh = true;
              }
            if (InCheck (!PlayerWhite))   // I can't save my King
              {
                lMessage->TextSet ("I SURRENDER");
                lMessage->VisibleSet (true);
              }
            else if (InCheck (PlayerWhite))
              {
                lMessage->TextSet ("IN CHECK");
                lMessage->VisibleSet (true);
              }
            else
              lMessage->VisibleSet (false);
          }
        if (PlayForMe)
          {
            PlayForMe = false;
            PlayScore = BestMove (PlayerWhite, 0);
            if (PlayScore != MININT)
              {
                cBoard->MoveFrom = BestA [0];
                cBoard->MoveTo = BestB [0];
                cBoard->MoveComplete = true;
              }
          }
        if (cBoard->MoveComplete)
          {
            memcpy (BoardOld, Board, sizeof (_Board));   // save board for undo
            sGOld = sG;   // and the graveyard
            CanUndo = true;
            cBoard->MoveStart = false;
            cBoard->MoveComplete = false;
            StrCat (&sS, '[');
            IntToStr (&sS, MoveCount);
            StrCat (&sS, "] ");
            CoordToStr (&sS, cBoard->MoveFrom);
            CoordToStr (&sS, cBoard->MoveTo);
            if (MoveValid (cBoard->MoveFrom, cBoard->MoveTo))
              {
                TakePiece (cBoard->MoveTo, &sS, &sG);
                sm = MovePiece (cBoard->MoveFrom, cBoard->MoveTo);
                if (sm != smNone)
                  StrCat (&sS, '+');
                StrCat (&sS, ' ');
                if (InCheck (PlayerWhite))   // You moved into / stayed in Check
                  {
                    memcpy (Board, BoardOld, sizeof (_Board));   // undo move
                    sG = sGOld;   // fix graves
                    CanUndo = false;
                    lMessage->TextSet ("Save Your King");
                    lMessage->VisibleSet (true);
                    StrCat (&sS, '\n');
                  }
                else
                  PCPlay = true;
              }
            else
              {
                StrCat (&sS, " Error\n");
                cBoard->MoveFrom.x = -1;
              }
            Refresh = true;
          }
        if (Refresh)
          {
            Refresh = false;
            cBoard->Invalidate (true);
            *sG = 0;
            lGraveyard->TextSet (Graveyard);
            *sS = 0;
            lLog->TextSet (Status);
          }
        if (FormsUpdate ())
          break;
      }
    while (FormList)
      delete (FormList);
    free (Status);
    return 0;
  }
