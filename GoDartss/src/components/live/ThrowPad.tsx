"use client";

/**
 * Pavé de saisie tactile d'une fléchette (valeur + multiplicateur).
 * Gros boutons (≥64px) pensés pour une tablette 10" basse qualité.
 * Réutilisé pour la saisie manuelle ET la correction d'une fléchette.
 */

import { useState } from "react";

type Props = {
  title?: string;
  initialValue?: number;
  initialMultiplier?: number;
  onSubmit: (value: number, multiplier: number) => void;
  onCancel?: () => void;
};

const NUMBERS = Array.from({ length: 20 }, (_, i) => i + 1);

export default function ThrowPad({
  title = "Saisir la fléchette",
  initialValue,
  initialMultiplier = 1,
  onSubmit,
  onCancel,
}: Props) {
  const [value, setValue] = useState<number | null>(initialValue ?? null);
  const [mult, setMult] = useState<number>(initialMultiplier);

  const isBullOrMiss = value === 0 || value === 25 || value === 50;
  const effectiveMult = isBullOrMiss ? 1 : mult;

  const pick = (v: number) => {
    setValue(v);
    if (v === 0 || v === 25 || v === 50) setMult(1);
  };

  const btn =
    "min-h-16 rounded-lg text-2xl font-bold flex items-center justify-center " +
    "active:scale-95 transition-transform select-none";

  return (
    <div className="fixed inset-0 z-50 flex items-end sm:items-center justify-center bg-black/70 p-2">
      <div className="w-full max-w-2xl rounded-2xl bg-gray-900 border border-gray-700 p-4">
        <div className="flex items-center justify-between mb-3">
          <h2 className="text-xl font-bold text-white">{title}</h2>
          <div className="text-2xl font-mono text-red-500">
            {value === null
              ? "—"
              : value === 0
              ? "MISS"
              : `${effectiveMult === 3 ? "T" : effectiveMult === 2 ? "D" : ""}${value}`}
          </div>
        </div>

        {/* Multiplicateur */}
        <div className="grid grid-cols-3 gap-2 mb-2">
          {[
            { m: 1, label: "Simple" },
            { m: 2, label: "Double" },
            { m: 3, label: "Triple" },
          ].map(({ m, label }) => (
            <button
              key={m}
              disabled={isBullOrMiss}
              onClick={() => setMult(m)}
              className={`${btn} ${
                effectiveMult === m && !isBullOrMiss
                  ? "bg-red-600 text-white"
                  : "bg-gray-800 text-gray-300"
              } ${isBullOrMiss ? "opacity-40" : ""}`}
            >
              {label}
            </button>
          ))}
        </div>

        {/* Numéros 1-20 */}
        <div className="grid grid-cols-5 gap-2 mb-2">
          {NUMBERS.map((n) => (
            <button
              key={n}
              onClick={() => pick(n)}
              className={`${btn} ${
                value === n ? "bg-red-600 text-white" : "bg-gray-800 text-white"
              }`}
            >
              {n}
            </button>
          ))}
        </div>

        {/* Bull / Miss */}
        <div className="grid grid-cols-3 gap-2 mb-4">
          <button onClick={() => pick(25)}
            className={`${btn} ${value === 25 ? "bg-red-600 text-white" : "bg-gray-800 text-green-400"}`}>
            25
          </button>
          <button onClick={() => pick(50)}
            className={`${btn} ${value === 50 ? "bg-red-600 text-white" : "bg-gray-800 text-red-400"}`}>
            Bull 50
          </button>
          <button onClick={() => pick(0)}
            className={`${btn} ${value === 0 ? "bg-red-600 text-white" : "bg-gray-800 text-gray-400"}`}>
            MISS
          </button>
        </div>

        {/* Actions */}
        <div className="grid grid-cols-2 gap-2">
          {onCancel && (
            <button onClick={onCancel}
              className={`${btn} bg-gray-700 text-white`}>
              Annuler
            </button>
          )}
          <button
            disabled={value === null}
            onClick={() => value !== null && onSubmit(value, effectiveMult)}
            className={`${btn} ${onCancel ? "" : "col-span-2"} ${
              value === null ? "bg-gray-700 opacity-40" : "bg-green-600"
            } text-white`}
          >
            Valider
          </button>
        </div>
      </div>
    </div>
  );
}
