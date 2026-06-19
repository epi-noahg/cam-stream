/*
  Warnings:

  - Added the required column `updatedAt` to the `Game` table without a default value. This is not possible if the table is not empty.

*/
-- CreateTable
CREATE TABLE "PlayerBadge" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "playerId" INTEGER NOT NULL,
    "type" TEXT NOT NULL,
    "earnedAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "gameId" INTEGER,
    "metadata" JSONB,
    CONSTRAINT "PlayerBadge_playerId_fkey" FOREIGN KEY ("playerId") REFERENCES "Player" ("id") ON DELETE RESTRICT ON UPDATE CASCADE
);

-- CreateTable
CREATE TABLE "PlayerStats" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "playerId" INTEGER NOT NULL,
    "averageScore" REAL NOT NULL DEFAULT 0,
    "totalThrows" INTEGER NOT NULL DEFAULT 0,
    "totalScore" INTEGER NOT NULL DEFAULT 0,
    "bull50Count" INTEGER NOT NULL DEFAULT 0,
    "bull25Count" INTEGER NOT NULL DEFAULT 0,
    "tripleCount" INTEGER NOT NULL DEFAULT 0,
    "doubleCount" INTEGER NOT NULL DEFAULT 0,
    "checkoutAttempts" INTEGER NOT NULL DEFAULT 0,
    "checkoutSuccess" INTEGER NOT NULL DEFAULT 0,
    "averageCheckoutScore" REAL NOT NULL DEFAULT 0,
    "bestThreeDartAverage" REAL NOT NULL DEFAULT 0,
    "longest180Series" INTEGER NOT NULL DEFAULT 0,
    "total180s" INTEGER NOT NULL DEFAULT 0,
    "updatedAt" DATETIME NOT NULL,
    CONSTRAINT "PlayerStats_playerId_fkey" FOREIGN KEY ("playerId") REFERENCES "Player" ("id") ON DELETE RESTRICT ON UPDATE CASCADE
);

-- RedefineTables
PRAGMA defer_foreign_keys=ON;
PRAGMA foreign_keys=OFF;
CREATE TABLE "new_Game" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "mode" TEXT NOT NULL,
    "status" TEXT NOT NULL DEFAULT 'WAITING',
    "startingScore" INTEGER,
    "doubleOut" BOOLEAN NOT NULL DEFAULT false,
    "doubleIn" BOOLEAN NOT NULL DEFAULT false,
    "currentState" TEXT,
    "winnerId" INTEGER,
    "winnerRank" JSONB,
    "pausedAt" DATETIME,
    "createdAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" DATETIME NOT NULL,
    "finishedAt" DATETIME,
    CONSTRAINT "Game_winnerId_fkey" FOREIGN KEY ("winnerId") REFERENCES "Player" ("id") ON DELETE SET NULL ON UPDATE CASCADE
);
INSERT INTO "new_Game" ("createdAt", "doubleIn", "doubleOut", "finishedAt", "id", "mode", "startingScore") SELECT "createdAt", "doubleIn", "doubleOut", "finishedAt", "id", "mode", "startingScore" FROM "Game";
DROP TABLE "Game";
ALTER TABLE "new_Game" RENAME TO "Game";
CREATE TABLE "new_Player" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "nickname" TEXT NOT NULL,
    "email" TEXT,
    "avatar" TEXT,
    "totalGames" INTEGER NOT NULL DEFAULT 0,
    "totalWins" INTEGER NOT NULL DEFAULT 0,
    "totalLosses" INTEGER NOT NULL DEFAULT 0,
    "bestFinish" INTEGER,
    "createdAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "lastActiveAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
INSERT INTO "new_Player" ("createdAt", "id", "nickname") SELECT "createdAt", "id", "nickname" FROM "Player";
DROP TABLE "Player";
ALTER TABLE "new_Player" RENAME TO "Player";
CREATE UNIQUE INDEX "Player_nickname_key" ON "Player"("nickname");
CREATE UNIQUE INDEX "Player_email_key" ON "Player"("email");
PRAGMA foreign_keys=ON;
PRAGMA defer_foreign_keys=OFF;

-- CreateIndex
CREATE UNIQUE INDEX "PlayerBadge_playerId_type_key" ON "PlayerBadge"("playerId", "type");

-- CreateIndex
CREATE UNIQUE INDEX "PlayerStats_playerId_key" ON "PlayerStats"("playerId");
