///////////////////////////////////////////////////////////////////////////////////////////////////
//
// STEWARTS CHESS PROGRAMME
//
// Author: Stewart Tunbridge, Pi Micros
// Email:  stewarttunbridge@gmail.com
// Copyright (c) 2024 Stewart Tunbridge, Pi Micros
//
///////////////////////////////////////////////////////////////////////////////////////////////////


const char Revision [] = "1.01";

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../Widgets/Widgets.hpp"
#include "../Widgets/WidgetsGrid.hpp"
#include "../Widgets/FileSelect.hpp"

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
bool Exit;
bool Load;
bool Save;
bool SaveLog;

bool PlayThreadWhite;
bool PlayThreadStarted;
bool PlayThreadFinished;
int PlayThreadScore;
int PlayThreadTime;

void *PlayThread (void *PlayWhite)
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
    return NULL;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BOARD VISUAL COMPONENT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _ChessBoard: public _Container
  {
    public:
      int Colours [2];
      bool MoveStart, MoveComplete;
      _Coord Move [2];   // Human move
      _Coord MovePC [2];
      bool MovePosShow;
      bool MovePos [8][8];
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
    MovePosShow = true;
    memset (MovePos, false, sizeof (MovePos));
  }

_ChessBoard::~_ChessBoard ()
  {
  }

const int cGreenGray = 0x9cf09c;
const int cBlueGray = 0xff9c9c;

extern int CopyGlyphPixelCount;

void _ChessBoard::DrawCustom ()
  {
    int dx, dy;
    _Rect Square;
    _Coord Center;
    int x, y, Pass;
    int c;
    char p_ [] = "?";
    _Font *FontTag;
    int cBG;
    int Width;
    //int t;//####
    //
    //t = ClockMS ();
    Time1 = Time2 = Time3 = 0;
    dx = Rect.Width / 8;
    dy = Rect.Height / 8;
    Width = Max (1, dx / 16);
    FontTag = Parent->FontFind ();   // Use the Forms "normal" font for Tags
    if (FontSet ("CHEQ_TT.TTF", dy * 4 / 5))   // fonts4free.net/chess-font.html
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
                        FontTag->ColourBG = c;
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
                        //char s [16], *ss;
                        //ss = s;
                        //NumToStr (&ss, Board [x][y] / pMoves);
                        //*ss = 0;
                        //TextOutAligned (FontTag, Square, s, aRight, aLeft);
                        FontTag->ColourBG = cBG;
                      }
                    Font->ColourBG = c;
                    p_ [0] = PieceToChar (Board [x][y]);
                    TextOutAligned (Font, Square, p_, aCenter, aCenter);
                    if (Board [x][y] & pChecked)
                      DrawCircle (Center.x, Center.y, Max (dx / 16, 1), cRed, ColourAdjust (cRed, 150), 1);
                    /* //Debug - testing pieces attributes
                    if ((int) Board [x][y] / pMoves > 0)
                      DrawLine (Square.x + dx, Square.y, Square.x, Square.y + dy, cBlue, 1);
                    */
                    if (MovePosShow && MovePos [x][y])
                      {
                        //DrawRectangle ({x_ - Width, y_ - Width, 2 * Width, 2 * Width}, cBlack, cBlack, cWhite);
                        DrawLine (Center.x - dx / 8, Center.y - dy / 8, Center.x + dx / 8, Center.y + dy / 8, cWhite, Width);
                        DrawLine (Center.x + dx / 8, Center.y - dy / 8, Center.x - dx / 8, Center.y + dy / 8, cWhite, Width);
                      }
                  }
              }
          if (Pass == 0)   // Draw User and PC move "arrows"
            {
              if (MovePC [0].x >= 0)
                DrawLine (MovePC [0].x * dx + dx / 2, (7 - MovePC [0].y) * dy + dy / 2, MovePC [1].x * dx + dx / 2, (7 - MovePC [1].y) * dy + dy / 2, cGreenGray, Width);
              if (Move [0].x >= 0)
                DrawLine (Move [0].x * dx + dx / 2, (7 - Move [0].y) * dy + dy / 2, Move [1].x * dx + dx / 2, (7 - Move [1].y) * dy + dy / 2, cBlueGray, Width);
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
            memset (MovePos, false, sizeof (MovePos));
            if (Piece (Board [Sel.x][Sel.y]) != pEmpty)
              {
                GetPieceMoves (Sel, Moves);
                m = Moves;
                while (m->x >= 0)   // for all moves
                  {
                    MovePos [m->x][m->y] = true;
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
              memset (MovePos, false, sizeof (MovePos));
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

_ChessBoard *cBoard;
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
      if (!b->Down && !PlayThreadStarted)
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

void ActionEdit (_Button *Button)
  {
    cBoard->MovePosShow = !Button->Down;
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
    //Menu->SelectionList->Die ();
  }

bool TakePiece (_Coord Pos, char **Logs, char *Graveyard)
  {
    _Piece p;
    char *g;
    //
    p = (_Piece) Board [Pos.x][Pos.y];
    if (Piece (p) != pEmpty)
      {
        if (Logs)
          StrCat (Logs, "**");
        if (Graveyard)
          {
            g = StrPos (Graveyard, '\n');
            if (g == NULL)
              {
                Graveyard [0] = '\n';
                Graveyard [1] = 0;
                g = Graveyard;
              }
            if (PieceWhite (p))
              StrInsert (g, PieceToChar (p));
            else
              StrAppend (Graveyard, PieceToChar (p));
          }
        return true;
      }
    return false;
  }

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
bool StrMatch (char **St, char *Target)
  {
    char *p;
    //
    p = *St;
    while (true)
      if (*Target == 0)   // found
        {
          *St = p;
          return true;
        }
      else if (*Target++ != *p++)
        return false;
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

const int LogsSize = 2000;
char *Logs;
char Graveyard [100];

bool GameSave (void)
  {
    char *Name;
    int f;   // file ID
    char Line [LogsSize + 100], *l;
    int x, y;
    bool Res;
    //
    Res = false;
    if (FileSelect (&Name, ".chess", true))
      {
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
            l = Line;
            StrCat (&l, "Graveyard\t");
            StrCat (&l, Graveyard);
            *l = 0;
            FileWriteLine_ (f, Line);
            l = Line;
            StrCat (&l, "Logs\t");
            StrCat (&l, Logs);
            *l = 0;
            FileWriteLine_ (f, Line);
            l = Line;
            StrCat (&l, "Moves\t");
            NumToStr (&l, MoveCount);
            *l = 0;
            FileWriteLine_ (f, Line);
            FileClose (f);
            free (Name);
            Res = true;
          }
      }
    return Res;
  }

void LogSave (void)
  {
    char *Name;
    int f;   // file ID
    //
    if (FileSelect (&Name, ".log", true))
      {
        f = FileOpen (Name, foWrite);
        if (f >= 0)
          {
            FileWrite (f, (byte *) Logs, StrLen (Logs));
            FileClose (f);
          }
      }
  }

bool GameLoad (void)
  {
    char *Name;
    int f;   // file ID
    int Size;
    char *Data, *dp, *dp_;
    int x, y;
    bool Res;
    //
    Res = false;
    if (FileSelect (&Name, ".chess", false))
      {
        f = FileOpen (Name, foRead);
        if (f >= 0)
          {
            Res = true;
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
                        Board [x][y] = StrGetHex (&dp);
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
                    strcpy (Graveyard, dp);
                    StrChangeChar (Graveyard, ',', '\n');
                  }
                else if (StrMatch (&dp, "Logs\t"))
                  {
                    strcpy (Logs, dp);
                    StrChangeChar (Logs, ',', '\n');
                  }
                else if (StrMatch (&dp, "Moves\t"))
                  MoveCount = StrGetNum (&dp);
                else
                  break;
                dp = dp_;
              }
            FileClose (f);
            free (Data);
          }
        free (Name);
      }
    return Res;
  }

int main_ (int argc, char *argv [])
  {
    _Coord FormSize = {650, 600};
    int BoardWidth = 500;
    int btn = 56;
    _Form *Form;
    _Container *Toolbar;
    _Button *bUndo, *bPlay, *bRestart, *bEdit, *bQuit;
    _Label *lGraveyard;
    _Label *lMessage;
    _Label *lLogs;
    _Label *lPCStats;
    _Wait *Wait;
    _MenuPopup *Menu;
    bool PCPlay;
    _Coord MovePC [2];
    //int PlayScore;
    int x, y;
    char *sLogs;
    char GraveyardOld [100];
    char PCStats [100], *pPC;
    _SpecialMove sm;
    _Board BoardOld;
    bool CanUndo;
    //
    DebugAddS ("===========Start Chess", Revision);
    BoardInit ();
    PlayerWhite = true;
    PCPlay = false;
    Logs = (char *) malloc (LogsSize);
    // Make the Main Form
    sLogs = Logs;   // Build the form Title
    StrCat (&sLogs, "Stewy's Chess Programme - ");
    StrCat (&sLogs, Revision);
    *sLogs = 0;
    Form = new _Form (Logs, {100, 100, FormSize.x, FormSize.y}, waResizable);
    sLogs = Logs;
    Form->Container->Colour = cYellow;
    Form->Container->ColourGrad = cOrange;
    Form->Container->FontSet ("ARI.ttf", 14);
    Form->Container->Bitmap = BitmapLoad (Form->Window, "Chess.bmp");
    y = 0;
    // build Toolbar
    Toolbar = new _Container (Form->Container, {0, y, 0, 45});
    Toolbar->Colour = ColourAdjust (cForm1, 80);
    x = 0;
    bProperties = new _Button (Toolbar, {x, 0, btn, 0}, "\e0\nSetup", (_Action) ActionProperties); x += btn;
    bProperties->Toggle = true;
    bUndo = new _Button (Toolbar, {x, 0, btn, 0}, "\e1\nUndo", (_Action) ActionUndo); x += btn;
    bPlay = new _Button (Toolbar, {x, 0, btn, 0}, "\e2\nPlay", (_Action) ActionPlay); x += btn;
    bRestart = new _Button (Toolbar, {x, 0, btn, 0}, "\e3\nRestart", (_Action) ActionRestart); x += btn;
    bEdit = new _Button (Toolbar, {x, 0, btn, 0}, "\e4\nEdit", (_Action) ActionEdit); x += btn;
    bEdit->Toggle = true;
    bQuit = new _Button (Toolbar, {x, 0, btn, 0}, "\e5\nQuit", (_Action) ActionQuit); x += btn;
    y += Toolbar->Rect.Height;
    lMessage = new _Label (Toolbar, {x, 0, 0, 0}, NULL, aCenter);
    lMessage->FontSet (NULL, 30, fsBold);
    lMessage->Colour = cRed;
    lMessage->VisibleSet (false);
    // Dead Pieces go to the Graveyard
    lGraveyard = new _Label (Form->Container, {0, y, BoardWidth, 60}, NULL, aLeft);
    lGraveyard->FontSet ("CHEQ_TT.TTF", lGraveyard->Rect.Height / 2 - 1);
    lGraveyard->RectLock |= rlRight;
    y += lGraveyard->Rect.Height;
    //
    x = 0;
    cBoard = new _ChessBoard (Form->Container, {x, y, BoardWidth, Form->Container->Rect.Height - y - 20});
    cBoard->RectLock |= rlRight + rlBottom;
    //cBoard->Colour = cGray;
    //y += cBoard->Rect.Height;
    x += BoardWidth;
    //
    lLogs = new _Label (Form->Container, {x + 8, Toolbar->Rect.Height, 0, lGraveyard->Rect.Height + cBoard->Rect.Height - 8}, NULL, aLeft, bNone);
    lLogs->RectLock = rlLeft | rlBottom;
    lLogs->AlignVert = aRight;   // = Bottom
    lLogs->ColourText = ColourAdjust (cAqua, 20); //ColourAdjust (cOrange, 70); cBrown;
    lLogs->FontSet ("ARI.ttf", 14, fsBold);
    y += cBoard->Rect.Height;
    //
    lPCStats = new _Label (Form->Container, {0, y, 0, 0}, NULL, aLeft);
    lPCStats->RectLock |= rlTop;
    lPCStats->Colour = cYellow;//cOrange;
    lPCStats->ColourGrad = cOrange;
    lPCStats->ColourText = ColourAdjust (cGreen, 30);
    //
    Wait = new _Wait (lLogs, {0, 0, 0, 0});
    Wait->VisibleSet (false);
    //
    Menu = new _MenuPopup (Form->Container, {}, "\e6Reload\t\e7Save\t\e7Save Log", ActionMenu);
    // Main Loop
    Restart = true;
    PlayForMe = PCPlay = CanUndo = false;
    while (!Exit)
      {
        usleep (100);
        //if (PlayThreadStarted)
        //  cBoard->Invalidate (true);
        if (sLogs - Logs + 50 > LogsSize)   // the problem of c strings is ...
          sLogs = Logs;
        if (Restart)
          {
            Restart = false;
            MoveCount = 1;
            BoardInit ();
            cBoard->Move [0].x = -1;
            cBoard->MovePC [0].x = -1;
            Graveyard [0] = 0;
            sLogs = Logs;
            lMessage->VisibleSet (false);
            lPCStats->TextSet (NULL);
            Refresh = true;
            Undo = CanUndo = false;
          }
        else if (Load)
          {
            Load = false;
            Form->Container->EnabledSet (false);
            if (GameLoad ())
              {
                sLogs = StrPos (Logs, (char) 0);
                CanUndo = false;
                cBoard->Move [0].x = -1;
                cBoard->MovePC [0].x = -1;
                lMessage->VisibleSet (false);
                Form->Container->EnabledSet (true);
                Refresh = true;
              }
          }
        else if (Save)
          {
            Save = false;
            Form->Container->EnabledSet (false);
            if (!GameSave ())
              StrCat (&sLogs, "*** ERROR SAVING FILE ***");
            Form->Container->EnabledSet (true);
          }
        else if (SaveLog)
          {
            SaveLog = false;
            LogSave ();
          }
        else if (Undo)
          {
            Undo = false;
            if (CanUndo)
              {
                CanUndo = false;
                memcpy (Board, BoardOld, sizeof (_Board));
                cBoard->Move [0].x = -1;
                cBoard->MovePC [0].x = -1;
                strcpy (Graveyard, GraveyardOld);
                MoveCount--;
                Refresh = true;
              }
          }
        else if (PlayThreadFinished)
          {
            PlayThreadFinished = false;
            Toolbar->EnabledSet (true);
            if (PlayThreadWhite == PlayerWhite)   // playing for the human
              if (PlayThreadScore == MININT)
                {
                  lMessage->TextSet ("No Moves, Give up");
                  lMessage->VisibleSet (true);
                }
              else
                {
                  cBoard->Move [0] = BestA [0];
                  cBoard->Move [1] = BestB [0];
                  cBoard->MoveComplete = true;
                }
            else   // playing for the PC
              {
                if (PlayThreadScore == MININT)
                  {
                    lMessage->TextSet ("I Surrender");
                    lMessage->VisibleSet (true);
                  }
                else
                  {
                    pPC = PCStats;
                    StrCat (&pPC, "PC Move Stats: ");
                    IntToStr (&pPC, MovesConsidered, DigitsCommas);
                    StrCat (&pPC, " moves.  Score ");
                    IntToStr (&pPC, PlayThreadScore, DigitsCommas);
                    StrCat (&pPC, ".  Time ");
                    NumToStrDecimals (&pPC, PlayThreadTime, 3);
                    StrCat (&pPC, " s.  ");
                    /*for (x = 0; x < DepthPlay; x++)
                      {
                        CoordToStr (&pPC, BestA [x]);
                        StrCat (&pPC, '-');
                        CoordToStr (&pPC, BestB [x]);
                        StrCat (&pPC, ' ');
                      }*/
                    *pPC = 0;
                    lPCStats->TextSet (PCStats);
                    DebugAdd (PCStats); //####
                    MovePC [0] = BestA [0];
                    MovePC [1] = BestB [0];
                    CoordToStr (&sLogs, MovePC [0]);
                    CoordToStr (&sLogs, MovePC [1]);
                    TakePiece (MovePC [1], &sLogs, Graveyard);
                    sm = MovePiece (MovePC [0], MovePC [1]);
                    cBoard->MovePC [0] = MovePC [0];
                    cBoard->MovePC [1] = MovePC [1];
                    cBoard->Invalidate (true);
                    if (sm != smNone)
                      StrCat (&sLogs, '+');
                    if (InCheck (PlayerWhite))
                      {
                        lMessage->TextSet ("IN CHECK");
                        lMessage->VisibleSet (true);
                      }
                    else
                      lMessage->VisibleSet (false);
                  }
                MoveCount++;
                StrCat (&sLogs, '\n');
                Refresh = true;
              }
            //cBoard->EnabledSet (true);
            Wait->VisibleSet (false);
          }
        else if (PCPlay)
          {
            PCPlay = false;
            Toolbar->EnabledSet (false);
            InCheck (!PlayerWhite);   // Mark PC King if in check
            StartThread (PlayThread, (void *) !PlayerWhite);
            Wait->ColourText = ColourAdjust (cGreen, 25);
            Wait->VisibleSet (true);
          }
        else if (PlayForMe)
          {
            PlayForMe = false;
            Toolbar->EnabledSet (false);
            StartThread (PlayThread, (void *) PlayerWhite);
            Wait->ColourText = ColourAdjust (cBlue, 50);
            Wait->VisibleSet (true);
          }
        else if (cBoard->MoveComplete)
          {
            cBoard->MoveStart = false;
            cBoard->MoveComplete = false;
            if (bEdit->Down)
              {
                TakePiece (cBoard->Move [1], NULL, Graveyard);
                Board [cBoard->Move [1].x][cBoard->Move [1].y] = Board [cBoard->Move [0].x][cBoard->Move [0].y];
                Board [cBoard->Move [0].x][cBoard->Move [0].y] = pEmpty;
              }
            else
              {
                memcpy (BoardOld, Board, sizeof (_Board));   // save board for undo
                strcpy (GraveyardOld, Graveyard);
                CanUndo = true;
                StrCat (&sLogs, '[');
                IntToStr (&sLogs, MoveCount);
                StrCat (&sLogs, "] ");
                CoordToStr (&sLogs, cBoard->Move [0]);
                CoordToStr (&sLogs, cBoard->Move [1]);
                if (MoveValid (cBoard->Move [0], cBoard->Move [1]))
                  {
                    TakePiece (cBoard->Move [1], &sLogs, Graveyard);
                    sm = MovePiece (cBoard->Move [0], cBoard->Move [1]);
                    if (sm != smNone)
                      StrCat (&sLogs, '+');
                    StrCat (&sLogs, ' ');
                    if (InCheck (PlayerWhite))   // You moved into / stayed in Check
                      {
                        memcpy (Board, BoardOld, sizeof (_Board));   // undo move
                        strcpy (Graveyard, GraveyardOld);
                        CanUndo = false;
                        lMessage->TextSet ("Save Your King");
                        lMessage->VisibleSet (true);
                        StrCat (&sLogs, " Error\n");
                      }
                    else
                      PCPlay = true;
                  }
                else
                  {
                    StrCat (&sLogs, " Error\n");
                    cBoard->Move [0].x = -1;
                  }
              }
            Refresh = true;
          }
        if (Refresh)
          {
            Refresh = false;
            cBoard->Invalidate (true);
            lGraveyard->TextSet (Graveyard);
            *sLogs = 0;
            lLogs->TextSet (Logs);
          }
        if (FormsUpdate ())
          break;
      }
    while (FormList)
      delete (FormList);
    free (Logs);
    return 0;
  }


/********************************************************************************************************************* JUNK
                Name = (char *) malloc (64);
                sN = Name;
                StrCat (&sN, "board");
                CurrentDateTimeToStr (&sN, true, true);
                StrCat (&sN, ".dat");
                *sN = 0;
*********************************************************************************************************************/
