-- CreateTable
CREATE TABLE "Player" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "nickname" TEXT NOT NULL,
    "createdAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- CreateTable
CREATE TABLE "GameParticipant" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "gameId" INTEGER NOT NULL,
    "playerId" INTEGER NOT NULL,
    "team" INTEGER NOT NULL DEFAULT 1,
    "order" INTEGER NOT NULL,
    CONSTRAINT "GameParticipant_gameId_fkey" FOREIGN KEY ("gameId") REFERENCES "Game" ("id") ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT "GameParticipant_playerId_fkey" FOREIGN KEY ("playerId") REFERENCES "Player" ("id") ON DELETE RESTRICT ON UPDATE CASCADE
);

-- CreateTable
CREATE TABLE "Game" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "mode" TEXT NOT NULL,
    "startingScore" INTEGER,
    "doubleOut" BOOLEAN NOT NULL DEFAULT false,
    "doubleIn" BOOLEAN NOT NULL DEFAULT false,
    "createdAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "finishedAt" DATETIME
);

-- CreateTable
CREATE TABLE "Turn" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "gameId" INTEGER NOT NULL,
    "participantId" INTEGER NOT NULL,
    "orderInGame" INTEGER NOT NULL,
    "createdAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "Turn_gameId_fkey" FOREIGN KEY ("gameId") REFERENCES "Game" ("id") ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT "Turn_participantId_fkey" FOREIGN KEY ("participantId") REFERENCES "GameParticipant" ("id") ON DELETE RESTRICT ON UPDATE CASCADE
);

-- CreateTable
CREATE TABLE "Throw" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "turnId" INTEGER NOT NULL,
    "value" INTEGER NOT NULL,
    "multiplier" INTEGER NOT NULL DEFAULT 1,
    "createdAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "Throw_turnId_fkey" FOREIGN KEY ("turnId") REFERENCES "Turn" ("id") ON DELETE CASCADE ON UPDATE CASCADE
);

-- CreateIndex
CREATE UNIQUE INDEX "Player_nickname_key" ON "Player"("nickname");

-- CreateIndex
CREATE UNIQUE INDEX "GameParticipant_gameId_playerId_key" ON "GameParticipant"("gameId", "playerId");
