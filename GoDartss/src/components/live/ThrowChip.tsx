"use client";

/**
 * Affichage stylé d'un lancer : grand chiffre + libellé (TRIPLE / DOUBLE /
 * BULL / RATÉ) en dessous, coloré selon le type. Remplace le texte "20×3".
 * Cliquable (si onTap) pour corriger via le pavé tactile.
 */

import type { Throw } from "@/lib/dartTypes";

function describe(t: Throw): { main: string; sub: string; color: string } {
  if (t.bust) return { main: "✕", sub: "BUST", color: "text-red-500" };
  if (t.value === 0) return { main: "–", sub: "RATÉ", color: "text-gray-500" };
  if (t.value === 50) return { main: "50", sub: "BULL", color: "text-red-400" };
  if (t.value === 25) return { main: "25", sub: "25", color: "text-green-400" };
  if (t.multiplier === 3) return { main: String(t.value), sub: "TRIPLE", color: "text-green-400" };
  if (t.multiplier === 2) return { main: String(t.value), sub: "DOUBLE", color: "text-sky-400" };
  return { main: String(t.value), sub: "SIMPLE", color: "text-gray-300" };
}

export default function ThrowChip({
  thr,
  size = "lg",
  onTap,
}: {
  thr?: Throw | null;
  size?: "sm" | "lg";
  onTap?: () => void;
}) {
  const lg = size === "lg";
  const box = lg ? "min-h-[64px] py-1" : "min-h-[40px]";
  const num = lg ? "text-3xl" : "text-lg";
  const sub = lg ? "text-[10px]" : "text-[8px]";

  if (!thr) {
    return (
      <div className={`flex-1 ${box} rounded-lg border border-dashed border-gray-700 flex items-center justify-center text-gray-700 ${num}`}>
        ·
      </div>
    );
  }

  const d = describe(thr);
  const Tag = onTap ? "button" : "div";
  return (
    <Tag
      onClick={onTap}
      className={`flex-1 ${box} rounded-lg bg-gray-800 border border-gray-700 flex flex-col items-center justify-center leading-none ${onTap ? "active:scale-95 cursor-pointer" : ""}`}
    >
      <span className={`${num} font-black ${d.color}`}>{d.main}</span>
      <span className={`${sub} font-bold tracking-[0.15em] text-gray-400 mt-0.5`}>{d.sub}</span>
    </Tag>
  );
}
