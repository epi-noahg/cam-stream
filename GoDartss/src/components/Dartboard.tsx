// src/components/Dartboard.tsx
"use client";

import { useRef, useState } from "react";
import { useGame } from "@/context/GameContext";
import { getCheckoutSuggestion } from "@/lib/checkout";
import { theme, cn } from "@/lib/theme";
import type { Throw } from "@/types/game";

interface DartboardProps {
  /** Square size in pixels – default 320 */
  size?: number;
}

/* ------------------------------------------------------------------ */
/*  CONSTANTS                                                         */
/* ------------------------------------------------------------------ */

const SECTORS = [
  20, 5, 12, 9, 14, 11, 8, 16, 7, 19,
  3, 17, 2, 15, 10, 6, 13, 4, 18, 1,
];

/** Radii expressed as percentages of board half‑width (100 = rim)    */
const R = {
  doubleOuter: 110,
  doubleInner: 95,
  singleOuter: 95,
  singleInnerOuter: 70,
  tripleOuter: 70,
  tripleInner: 55,
  singleInner: 55,
  bullOuter: 19,
  bullInner: 9,
  label: 118,
};

/** Half‑width of the SVG viewBox (must match `viewBox="-140 -140 280 280"`) */
const VIEWBOX_HALF = 140;

/** Rayon de la zone OUT (au-delà de la double outer) */
const R_OUT = 127.5;


/* Colour helpers */
const RED   = "#ff0037";
const GREEN = "#009245";
const BLACK = "#1b1b1b";
const BLACK_LIGHT = "#0c0c0cff"; // slightly lighter for hover highlight
const WHITE = "#f2f2f2";
const SPIDER = "#222";

/** Opacity applied to non‑hovered elements */
const DIM_OPACITY = 0.5;

/* ------------------------------------------------------------------ */
/*  TYPES & HELPERS                                                   */
/* ------------------------------------------------------------------ */

type Ring =
  | "bullInner"
  | "bullOuter"
  | "double"
  | "singleOuter"
  | "triple"
  | "singleInner";

/** Return the logical ring for a relative radius on the 0‑100 scale */
const ringForRadius = (rRel: number): Ring | null => {
  if (rRel <= R.bullInner) return "bullInner";
  if (rRel <= R.bullOuter) return "bullOuter";
  if (rRel >= R.doubleInner && rRel <= R.doubleOuter) return "double";
  if (rRel >= R.tripleOuter && rRel < R.doubleInner) return "singleOuter";
  if (rRel >= R.tripleInner && rRel < R.tripleOuter) return "triple";
  if (rRel > R.bullOuter && rRel < R.tripleInner) return "singleInner";
  return null;
};

/* ------------------------------------------------------------------ */
/*  UTILS                                                             */
/* ------------------------------------------------------------------ */

/** Round to 6 decimals so Node & Browser stringify identically */
const fmt = (n: number) => n.toFixed(6);

const deg2rad = (d: number) => (d * Math.PI) / 180;
/** Cartesian coordinates (origin at centre, y up) */
const polar = (r: number, deg: number) => {
  const rad = deg2rad(deg);
  return [r * Math.cos(rad), r * Math.sin(rad)];
};

/** Path describing a ring segment (wedge) between two radii */
const wedgePath = (
  r0: number,
  r1: number,
  a0: number,
  a1: number,
  sweep = 1,
) => {
  const [x1, y1] = polar(r1, a0);
  const [x2, y2] = polar(r1, a1);
  const [x3, y3] = polar(r0, a1);
  const [x4, y4] = polar(r0, a0);

  return [
    `M${fmt(x1)} ${fmt(y1)}`,
    `A${r1} ${r1} 0 0 ${sweep} ${fmt(x2)} ${fmt(y2)}`,
    `L${fmt(x3)} ${fmt(y3)}`,
    `A${r0} ${r0} 0 0 ${1 - sweep} ${fmt(x4)} ${fmt(y4)}`,
    "Z",
  ].join(" ");
};

/* ------------------------------------------------------------------ */
/*  COMPONENT                                                         */
/* ------------------------------------------------------------------ */

export default function Dartboard({ size }: DartboardProps) {
  const { dispatch } = useGame();
  const { state } = useGame();
  const svgRef       = useRef<SVGSVGElement>(null);
  const [hover, setHover] = useState<{ 
    sector: number | null; 
    ring: Ring | null;
    value?: number;
    multiplier?: number; 
  }>({ sector: null, ring: null });

  // Check if current player has already finished
  const currentPlayer = state.players[state.currentIndex];
  const isCurrentPlayerFinished = currentPlayer ? state.finishedPlayers.includes(currentPlayer.id) : false;

  /* Scoring click --------------------------------------------------- */
  const handleThrow = (value: number, multiplier: 1 | 2 | 3) => {
    dispatch({ type: "THROW", payload: { value, multiplier } });
  };

  const handleClick = (e: React.MouseEvent<SVGSVGElement>) => {
    if (!svgRef.current) return;
    
    // Prevent clicks if no current player or current player has already finished
    if (!currentPlayer || isCurrentPlayerFinished) return;

    const rect = svgRef.current.getBoundingClientRect();
    const cx = rect.width / 2;
    const x = e.clientX - rect.left - cx;
    const y = -(e.clientY - rect.top - cx); // y upwards
    const rAbs = Math.hypot(x, y);
    const boardRadiusPx = cx * (R.doubleOuter / VIEWBOX_HALF); // pixels at rim (110 units)
    const rRel = (rAbs / boardRadiusPx) * R.doubleOuter;       // échelle relative aux rayons R

    if (rRel > R_OUT) return;

    const ring = ringForRadius(rRel);

    /* Bulls ---------------------------------------------------------- */
    if (ring === "bullInner") {
      handleThrow(50, 1);
      return;
    }
    if (ring === "bullOuter") {
      handleThrow(25, 1);
      return;
    }
    if (!ring) {
      handleThrow(0, 1); // OUT: lancer raté
      return;
    }

    /* Sector rings --------------------------------------------------- */
    let mult: 1 | 2 | 3 = 1;
    if (ring === "double") mult = 2;
    else if (ring === "triple") mult = 3;

    let angle = (Math.atan2(y, x) * 180) / Math.PI;
    angle = (angle + 360 - 90) % 360;
    const idx = Math.floor((angle + 9) / 18) % 20;
    const val = SECTORS[idx];

    handleThrow(val, mult);
  };

  /* Pointer‑move (hover) --------------------------------------------- */
  const handlePointerMove = (e: React.PointerEvent<SVGSVGElement>) => {
    if (!svgRef.current) return;

    const rect = svgRef.current.getBoundingClientRect();
    const cx = rect.width / 2;
    const x = e.clientX - rect.left - cx;
    const y = -(e.clientY - rect.top - cx);
    const boardRadiusPx = cx * (R.doubleOuter / VIEWBOX_HALF);
    const rAbs = Math.hypot(x, y);
    const rRel = (rAbs / boardRadiusPx) * R.doubleOuter;

    if (rRel > R_OUT) {
      setHover({ sector: null, ring: null });
      return;
    }

    const ring = ringForRadius(rRel);

    let sectorIdx: number | null = null;
    let value: number | undefined;
    let multiplier: number | undefined;

    if (ring && ring !== "bullInner" && ring !== "bullOuter") {
      let angle = (Math.atan2(y, x) * 180) / Math.PI;
      angle = (angle + 360 - 90) % 360;
      sectorIdx = Math.floor((angle + 9) / 18) % 20;
      value = SECTORS[sectorIdx];
      
      switch (ring) {
        case "double": multiplier = 2; break;
        case "triple": multiplier = 3; break;
        default: multiplier = 1; break;
      }
    } else if (ring === "bullInner") {
      value = 50;
      multiplier = 1;
    } else if (ring === "bullOuter") {
      value = 25;
      multiplier = 1;
    } else if (!ring) {
      value = 0;
      multiplier = 1;
    }

    setHover({ sector: sectorIdx, ring, value, multiplier });
  };

  function getThrowPosition(thr: Throw) {
    if (!thr) return { x: 0, y: 0, label: "" };
    let ringRadius: number;
    if (thr.value === 25 || thr.value === 50) {
      ringRadius = thr.value === 50 ? R.bullInner : R.bullOuter;
      return { x: 0, y: 0, label: thr.value.toString() };
    }
    switch (thr.multiplier) {
      case 3:
        ringRadius = (R.tripleInner + R.tripleOuter) / 2;
        break;
      case 2:
        ringRadius = (R.doubleInner + R.doubleOuter) / 2;
        break;
      default:
        ringRadius = (R.tripleOuter + R.doubleInner) / 2;
        break;
    }
    const sectorIndex = SECTORS.indexOf(thr.value);
    const angleDeg = -90 - 9 - sectorIndex * 18 + 9; // centre secteur
    const rad = (angleDeg * Math.PI) / 180;
    const x = ringRadius * Math.cos(rad);
    const y = ringRadius * Math.sin(rad);
    let label = "";
    if (thr.multiplier === 2) label = "D" + thr.value;
    else if (thr.multiplier === 3) label = "T" + thr.value;
    else label = thr.value.toString();
    return { x, y, label };
  }

  /* ---------------------------------------------------------------- */

  // Fonction pour formater l'affichage du hover - version élégante
  const formatHoverDisplay = () => {
    if (!hover.value && hover.value !== 0) return { main: "", sub: "" };
    
    if (hover.value === 0) {
      return { main: "0", sub: "Raté" };
    }
    
    if (hover.value === 25) {
      return { main: "25", sub: "Bull Simple" };
    }
    
    if (hover.value === 50) {
      return { main: "50", sub: "Bull Double" };
    }
    
    const points = hover.value * (hover.multiplier || 1);
    const main = points.toString();
    let sub = "";
    
    if (hover.multiplier === 2) {
      sub = `Double ${hover.value}`;
    } else if (hover.multiplier === 3) {
      sub = `Triple ${hover.value}`;
    } else {
      sub = `Simple ${hover.value}`;
    }
    
    return { main, sub };
  };

  return (
    <div className="relative w-full h-full flex items-center justify-center">
      {/* Indicateur hover en haut de l'écran */}
      {hover.value !== undefined && (() => {
        const display = formatHoverDisplay();
        return (
          <div className="absolute -top-16 sm:-top-20 left-1/2 transform -translate-x-1/2 z-50 pointer-events-none">
            <div className="animate-in fade-in-0 zoom-in-95 duration-200">
              {/* Carte élégante */}
              <div className={cn(
                "relative px-3 py-2 sm:px-4 rounded-lg shadow-lg",
                "bg-white/95 backdrop-blur-md border border-gray-200/50",
                "dark:bg-gray-900/95 dark:border-gray-700/50",
                "text-center min-w-[80px] sm:min-w-[100px]"
              )}>
                {/* Glow effect subtil */}
                <div className="absolute inset-0 rounded-lg bg-gradient-to-r from-red-500/10 via-transparent to-red-500/10 opacity-50" />
                
                {/* Contenu */}
                <div className="relative">
                  {/* Score principal */}
                  <div className={cn(
                    "text-base sm:text-lg font-bold tracking-tight leading-none",
                    hover.value === 0 ? "text-gray-500" :
                    hover.multiplier === 2 ? "text-blue-600" :
                    hover.multiplier === 3 ? "text-green-600" :
                    "text-gray-900 dark:text-white"
                  )}>
                    {display.main}
                  </div>
                  
                  {/* Description */}
                  <div className={cn(
                    "text-xs font-medium leading-none mt-1",
                    "text-gray-600 dark:text-gray-400"
                  )}>
                    {display.sub}
                  </div>
                </div>
              </div>
            </div>
          </div>
        );
      })()}
      
      <svg
      ref={svgRef}
      onClick={handleClick}
      onPointerMove={handlePointerMove}
      viewBox="-140 -140 280 280"
      width={size}
      height={size}
      className={`w-full h-full max-w-[90vw] max-h-[90vw] select-none ${isCurrentPlayerFinished ? 'cursor-not-allowed opacity-50' : 'cursor-pointer'}`}
      style={size ? { width: size, height: size } : undefined}
      aria-label="Dartboard"
    >
      <defs>
        <filter id="hoverGlow" x="-50%" y="-50%" width="200%" height="200%">
          <feGaussianBlur stdDeviation="5" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
        {/* Pulsing gradient for checkout suggestion */}
        <radialGradient id="pulseGradient" r="60%" cx="50%" cy="50%">
          <stop offset="0%" stopColor="#3b82f6" stopOpacity="0.8" />
          <stop offset="100%" stopColor="#3b82f6" stopOpacity="0" />
        </radialGradient>
        <filter id="pulseBlur" x="-50%" y="-50%" width="200%" height="200%">
          <feGaussianBlur stdDeviation="4" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>
      { /* OUT ZONE - Background noir avec hover */ }
      <circle
        cx="0"
        cy="0"
        r={R_OUT}
        fill={hover.ring === null && hover.value === 0 ? "#333" : "#000"}
        stroke="#fff"
        strokeWidth="2"
        opacity={hover.ring ? (hover.ring === null ? 1 : DIM_OPACITY) : 1}
        filter={hover.ring === null && hover.value === 0 ? "url(#hoverGlow)" : undefined}
      />
      {/* Back plate */}
      <circle cx="0" cy="0" r={R.doubleOuter} fill={BLACK} stroke="#000" strokeWidth="0.6" />

      {/* 20 ring segments */}
      {SECTORS.map((v, i) => {
        const start = -90 - 9 - i * 18;  // centre 20 at −90° (12 o’clock)
        const end   = start + 18;
        const even  = i % 2 === 0;
        const activeRing = hover.sector === i ? hover.ring : null;

        /* Double */
        const doubleFill = even ? RED : GREEN;
        /* Single outer */
        const singleOuterFill = even ? BLACK : WHITE;
        /* Triple */
        const tripleFill = even ? RED : GREEN;
        /* Single inner */
        const singleInnerFill = even ? BLACK : WHITE;

        return (
          <g
            key={i}
          >
            {/* Double ring */}
            <path
              d={wedgePath(
                R.doubleInner,
                R.doubleOuter,
                start,
                end,
              )}
              fill={doubleFill}
              stroke={SPIDER}
              strokeWidth="0.25"
              filter={activeRing === "double" ? "url(#hoverGlow)" : undefined}
              opacity={hover.ring ? (activeRing === "double" ? 1 : DIM_OPACITY) : 1}
            />
            {/* Outer single */}
            <path
              d={wedgePath(
                R.singleInnerOuter,
                R.singleOuter,
                start,
                end,
              )}
              fill={
                activeRing === "singleOuter" && singleOuterFill === BLACK
                  ? BLACK_LIGHT
                  : singleOuterFill
              }
              stroke={SPIDER}
              strokeWidth="0.25"
              filter={activeRing === "singleOuter" ? "url(#hoverGlow)" : undefined}
              opacity={hover.ring ? (activeRing === "singleOuter" ? 1 : DIM_OPACITY) : 1}
            />
            {/* Triple */}
            <path
              d={wedgePath(
                R.tripleInner,
                R.tripleOuter,
                start,
                end,
              )}
              fill={tripleFill}
              stroke={SPIDER}
              strokeWidth="0.25"
              filter={activeRing === "triple" ? "url(#hoverGlow)" : undefined}
              opacity={hover.ring ? (activeRing === "triple" ? 1 : DIM_OPACITY) : 1}
            />
            {/* Inner single */}
            <path
              d={wedgePath(
                R.bullOuter,
                R.singleInner,
                start,
                end,
              )}
              fill={
                activeRing === "singleInner" && singleInnerFill === BLACK
                  ? BLACK_LIGHT
                  : singleInnerFill
              }
              stroke={SPIDER}
              strokeWidth="0.25"
              filter={activeRing === "singleInner" ? "url(#hoverGlow)" : undefined}
              opacity={hover.ring ? (activeRing === "singleInner" ? 1 : DIM_OPACITY) : 1}
            />

            {/* Number labels */}
            {(() => {
              const [x, y] = polar(R.label, (start + end) / 2);
              return (
                <text
                  x={x.toPrecision(6)}
                  y={y.toPrecision(6)}
                  fill="#fff"
                  fontSize="9"
                  fontWeight="700"
                  textAnchor="middle"
                  dominantBaseline="middle"
                  paintOrder="stroke"
                  stroke="#000"
                  strokeWidth="1"
                >
                  {v}
                </text>
              );
            })()}
          </g>
        );
      })}

      {/* Outer bull */}
      <circle
        cx="0"
        cy="0"
        r={R.bullOuter}
        fill={GREEN}
        stroke="#000"
        strokeWidth="0.5"
        filter={hover.ring === "bullOuter" ? "url(#hoverGlow)" : undefined}
        opacity={hover.ring ? (hover.ring === "bullOuter" ? 1 : DIM_OPACITY) : 1}
      />
      {/* Inner bull */}
      <circle
        cx="0"
        cy="0"
        r={R.bullInner}
        fill={RED}
        stroke="#000"
        strokeWidth="0.5"
        filter={hover.ring === "bullInner" ? "url(#hoverGlow)" : undefined}
        opacity={hover.ring ? (hover.ring === "bullInner" ? 1 : DIM_OPACITY) : 1}
      />
      {state && (
        (() => {
          if (!currentPlayer) return null;
          if (isCurrentPlayerFinished) return null;

          // « Prochain bon coup » selon le mode de jeu. L'état serveur expose
          // `mode` et, par joueur, `marks` (Cricket) / `target` (Around the
          // Clock) — absents du type front legacy, lus via un cast léger.
          const g = state as unknown as {
            mode?: "X01" | "CRICKET" | "ROUND_THE_CLOCK";
          };
          const cp = currentPlayer as unknown as { marks?: number[]; target?: number };
          const mode = g.mode ?? "X01";

          let suggestion: Throw[] | null = null;
          if (mode === "CRICKET") {
            // Vise les cibles encore ouvertes (jusqu'à 3) : triple pour fermer
            // vite, simple pour le bull.
            const order = [20, 19, 18, 17, 16, 15, 25];
            const open: Throw[] = [];
            for (let i = 0; i < order.length && open.length < 3; i++) {
              if ((cp.marks?.[i] ?? 0) < 3) {
                const v = order[i];
                open.push(v === 25 ? { value: 25, multiplier: 1 } : { value: v, multiplier: 3 });
              }
            }
            suggestion = open.length ? open : null;
          } else if (mode === "ROUND_THE_CLOCK") {
            const t = cp.target ?? 0;
            suggestion = t >= 1 && t <= 20 ? [{ value: t, multiplier: 1 }] : null;
          } else {
            suggestion = getCheckoutSuggestion(
              currentPlayer.score,
              3 - state.dartIndex,
              state.options.outType
            );
          }
          return (
            suggestion &&
            suggestion.map((thr, i) => {
              const { x, y, label } = getThrowPosition(thr);
              return (
                <g key={"checkout-" + i} opacity={0.9}>
                  <circle
                    cx={x}
                    cy={y}
                    r={12}
                    fill="url(#pulseGradient)"
                  >
                    <animate
                      attributeName="r"
                      values="12;16;12"
                      dur="1.2s"
                      repeatCount="indefinite"
                    />
                    <animate
                      attributeName="opacity"
                      values="0.8;0.2;0.8"
                      dur="1.2s"
                      repeatCount="indefinite"
                    />
                  </circle>
                  <circle
                    cx={x}
                    cy={y}
                    r={7}
                    fill="#3b82f6"
                    stroke="#2563eb"
                    strokeWidth={1.5}
                    filter="url(#hoverGlow)"
                  >
                    <animateTransform
                      attributeName="transform"
                      type="scale"
                      from="0"
                      to="1"
                      dur="0.4s"
                      additive="replace"
                      fill="freeze"
                    />
                  </circle>
                  <text
                    x={x}
                    y={y + 2.5}
                    fill="#fff"
                    fontWeight="bold"
                    fontSize="7"
                    textAnchor="middle"
                    style={{ pointerEvents: "none" }}
                  >
                    {label}
                  </text>
                </g>
              );
            })
          );
        })()
      )}
    </svg>
    </div>
  );
}