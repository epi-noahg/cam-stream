"use client";

import { useRef, useState } from "react";
import { useGame } from "@/context/GameContext";
import { getCheckoutSuggestion } from "@/lib/checkout";
import { cn } from "@/lib/theme";
import type { Throw } from "@/types/game";

interface DartboardV2Props {
  /** Square size in pixels – default 400 */
  size?: number;
}

/* ------------------------------------------------------------------ */
/*  CONSTANTS OPTIMISÉS POUR L'ACCESSIBILITÉ MOBILE                 */
/* ------------------------------------------------------------------ */

const SECTORS = [
  20, 1, 18, 4, 13, 6, 10, 15, 2, 17,
  3, 19, 7, 16, 8, 11, 14, 9, 12, 5,
];

/** Radii optimisés pour faciliter les clics tactiles */
const R = {
  // Zone OUT agrandie (zone noire externe)
  outerRim: 100,
  outZone: 95,
  
  // Zones de scoring agrandies pour le tactile
  doubleOuter: 95,
  doubleInner: 85,
  singleOuter: 85,
  singleInnerOuter: 60,
  tripleOuter: 60,
  tripleInner: 45,
  singleInner: 45,
  
  // Bulls agrandis
  bullOuter: 18,
  bullInner: 10,
  
  // Labels repositionnés
  label: 110,
};

/** Half‑width of the SVG viewBox */
const VIEWBOX_HALF = 120; // Agrandi pour accommoder la zone OUT

/* Couleurs réalistes d'une vraie cible */
const DARTBOARD_COLORS = {
  RED: "#dc2626",
  GREEN: "#16a34a", 
  BLACK: "#000000",
  WHITE: "#ffffff",
  CREAM: "#fefce8",
  SPIDER: "#1f2937",
  OUT_ZONE: "#000000", // Zone OUT en noir profond
  HOVER_GLOW: "#ef4444",
};

/** Opacity pour les éléments non-survolés */
const DIM_OPACITY = 0.3;

/* ------------------------------------------------------------------ */
/*  TYPES & HELPERS                                                   */
/* ------------------------------------------------------------------ */

type Ring =
  | "bullInner"
  | "bullOuter" 
  | "double"
  | "singleOuter"
  | "triple"
  | "singleInner"
  | "out";

/** Détermine la zone pour un rayon relatif */
const ringForRadius = (rRel: number): Ring | null => {
  if (rRel > R.doubleOuter) return "out";
  if (rRel <= R.bullInner) return "bullInner";
  if (rRel <= R.bullOuter) return "bullOuter";
  if (rRel >= R.doubleInner && rRel <= R.doubleOuter) return "double";
  if (rRel >= R.tripleOuter && rRel < R.doubleInner) return "singleOuter";
  if (rRel >= R.tripleInner && rRel < R.tripleOuter) return "triple";
  if (rRel > R.bullOuter && rRel < R.tripleInner) return "singleInner";
  return null;
};

/* ------------------------------------------------------------------ */
/*  UTILS GÉOMÉTRIQUES                                               */
/* ------------------------------------------------------------------ */

const fmt = (n: number) => n.toFixed(6);
const deg2rad = (d: number) => (d * Math.PI) / 180;

const polar = (r: number, deg: number) => {
  const rad = deg2rad(deg);
  return [r * Math.cos(rad), r * Math.sin(rad)];
};

/** Path pour un segment d'anneau (wedge) avec zones tactiles agrandies */
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
/*  COMPONENT PRINCIPAL                                               */
/* ------------------------------------------------------------------ */

export default function DartboardV2({ size = 400 }: DartboardV2Props) {
  const { dispatch, state } = useGame();
  const svgRef = useRef<SVGSVGElement>(null);
  const [hover, setHover] = useState<{ 
    sector: number | null; 
    ring: Ring | null;
    value?: number;
    multiplier?: number;
  }>({ sector: null, ring: null });

  // Vérification si le joueur actuel a terminé
  const currentPlayer = state.players[state.currentIndex];
  const isCurrentPlayerFinished = currentPlayer ? state.finishedPlayers.includes(currentPlayer.id) : false;

  /* Gestion des lancers -------------------------------------------- */
  const handleThrow = (value: number, multiplier: 1 | 2 | 3) => {
    dispatch({ type: "THROW", payload: { value, multiplier } });
  };

  /* Gestion des clics avec zones tactiles agrandies */
  const handleClick = (e: React.MouseEvent<SVGSVGElement>) => {
    if (!svgRef.current || !currentPlayer || isCurrentPlayerFinished) return;

    const rect = svgRef.current.getBoundingClientRect();
    const cx = rect.width / 2;
    const x = e.clientX - rect.left - cx;
    const y = -(e.clientY - rect.top - cx);
    const rAbs = Math.hypot(x, y);
    const boardRadiusPx = cx * (R.outerRim / VIEWBOX_HALF);
    const rRel = (rAbs / boardRadiusPx) * 100;

    const ring = ringForRadius(rRel);

    /* Zone OUT (agrandie et noire) */
    if (ring === "out" || !ring) {
      handleThrow(0, 1);
      return;
    }

    /* Bulls (agrandis) */
    if (ring === "bullInner") {
      handleThrow(50, 1);
      return;
    }
    if (ring === "bullOuter") {
      handleThrow(25, 1);
      return;
    }

    /* Zones de scoring avec calcul de secteur */
    let mult: 1 | 2 | 3 = 1;
    if (ring === "double") mult = 2;
    else if (ring === "triple") mult = 3;

    let angle = (Math.atan2(y, x) * 180) / Math.PI;
    angle = (angle + 360 - 90) % 360;
    const idx = Math.floor((angle + 9) / 18) % 20;
    const val = SECTORS[idx];

    handleThrow(val, mult);
  };

  /* Gestion du survol avec feedback visuel amélioré */
  const handlePointerMove = (e: React.PointerEvent<SVGSVGElement>) => {
    if (!svgRef.current) return;

    const rect = svgRef.current.getBoundingClientRect();
    const cx = rect.width / 2;
    const x = e.clientX - rect.left - cx;
    const y = -(e.clientY - rect.top - cx);
    const boardRadiusPx = cx * (R.outerRim / VIEWBOX_HALF);
    const rAbs = Math.hypot(x, y);
    const rRel = (rAbs / boardRadiusPx) * 100;

    const ring = ringForRadius(rRel);

    let sectorIdx: number | null = null;
    let value: number | undefined;
    let multiplier: number | undefined;

    if (ring && ring !== "bullInner" && ring !== "bullOuter" && ring !== "out") {
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
    } else if (ring === "out") {
      value = 0;
      multiplier = 1;
    }

    setHover({ sector: sectorIdx, ring, value, multiplier });
  };

  const handlePointerLeave = () => {
    setHover({ sector: null, ring: null });
  };

  /* Calcul position des suggestions de checkout */
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
    const angleDeg = -90 - 9 - sectorIndex * 18 + 9;
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

  return (
    <div className="relative">

      <svg
        ref={svgRef}
        onClick={handleClick}
        onPointerMove={handlePointerMove}
        onPointerLeave={handlePointerLeave}
        viewBox={`-${VIEWBOX_HALF} -${VIEWBOX_HALF} ${VIEWBOX_HALF * 2} ${VIEWBOX_HALF * 2}`}
        width={size}
        height={size}
        className={cn(
          "select-none transition-opacity duration-200",
          isCurrentPlayerFinished ? "cursor-not-allowed opacity-50" : "cursor-pointer"
        )}
        aria-label="Cible de fléchettes optimisée"
      >
        <defs>
          {/* Filtre d'éclat pour le survol */}
          <filter id="hoverGlow" x="-50%" y="-50%" width="200%" height="200%">
            <feGaussianBlur stdDeviation="2" result="blur" />
            <feColorMatrix values="1 0 0 0 0.93  0 1 0 0 0.27  0 0 1 0 0.15  0 0 0 1 0" />
            <feMerge>
              <feMergeNode in="blur" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
          
          {/* Gradient pour suggestions de checkout */}
          <radialGradient id="checkoutGradient" r="60%" cx="50%" cy="50%">
            <stop offset="0%" stopColor="#3b82f6" stopOpacity="0.9" />
            <stop offset="100%" stopColor="#1d4ed8" stopOpacity="0.1" />
          </radialGradient>
        </defs>

        {/* Zone OUT agrandie et noire (comme une vraie cible) */}
        <circle 
          cx="0" 
          cy="0" 
          r={R.outerRim} 
          fill={DARTBOARD_COLORS.OUT_ZONE}
          stroke={DARTBOARD_COLORS.WHITE}
          strokeWidth="2"
        />

        {/* Fond principal de la cible */}
        <circle 
          cx="0" 
          cy="0" 
          r={R.doubleOuter} 
          fill={DARTBOARD_COLORS.CREAM}
          stroke={DARTBOARD_COLORS.SPIDER}
          strokeWidth="1"
        />

        {/* 20 segments de scoring avec zones tactiles agrandies */}
        {SECTORS.map((v, i) => {
          const start = -90 - 9 - i * 18;
          const end = start + 18;
          const even = i % 2 === 0;
          const activeRing = hover.sector === i ? hover.ring : null;

          /* Couleurs alternées comme vraie cible */
          const doubleFill = even ? DARTBOARD_COLORS.RED : DARTBOARD_COLORS.GREEN;
          const singleOuterFill = even ? DARTBOARD_COLORS.BLACK : DARTBOARD_COLORS.WHITE;
          const tripleFill = even ? DARTBOARD_COLORS.RED : DARTBOARD_COLORS.GREEN;
          const singleInnerFill = even ? DARTBOARD_COLORS.BLACK : DARTBOARD_COLORS.WHITE;

          return (
            <g key={i}>
              {/* Double ring (zone externe) */}
              <path
                d={wedgePath(R.doubleInner, R.doubleOuter, start, end)}
                fill={doubleFill}
                stroke={DARTBOARD_COLORS.SPIDER}
                strokeWidth="0.5"
                filter={activeRing === "double" ? "url(#hoverGlow)" : undefined}
                opacity={hover.ring && hover.sector !== null ? (activeRing === "double" ? 1 : DIM_OPACITY) : 1}
              />
              
              {/* Single outer */}
              <path
                d={wedgePath(R.singleInnerOuter, R.singleOuter, start, end)}
                fill={singleOuterFill}
                stroke={DARTBOARD_COLORS.SPIDER}
                strokeWidth="0.5"
                filter={activeRing === "singleOuter" ? "url(#hoverGlow)" : undefined}
                opacity={hover.ring && hover.sector !== null ? (activeRing === "singleOuter" ? 1 : DIM_OPACITY) : 1}
              />
              
              {/* Triple ring */}
              <path
                d={wedgePath(R.tripleInner, R.tripleOuter, start, end)}
                fill={tripleFill}
                stroke={DARTBOARD_COLORS.SPIDER}
                strokeWidth="0.5"
                filter={activeRing === "triple" ? "url(#hoverGlow)" : undefined}
                opacity={hover.ring && hover.sector !== null ? (activeRing === "triple" ? 1 : DIM_OPACITY) : 1}
              />
              
              {/* Single inner */}
              <path
                d={wedgePath(R.bullOuter, R.singleInner, start, end)}
                fill={singleInnerFill}
                stroke={DARTBOARD_COLORS.SPIDER}
                strokeWidth="0.5"
                filter={activeRing === "singleInner" ? "url(#hoverGlow)" : undefined}
                opacity={hover.ring && hover.sector !== null ? (activeRing === "singleInner" ? 1 : DIM_OPACITY) : 1}
              />

              {/* Numéros de secteur agrandis */}
              {(() => {
                const [x, y] = polar(R.label, (start + end) / 2);
                return (
                  <text
                    x={x.toPrecision(6)}
                    y={y.toPrecision(6)}
                    fill={DARTBOARD_COLORS.WHITE}
                    fontSize="12"
                    fontWeight="900"
                    textAnchor="middle"
                    dominantBaseline="middle"
                    paintOrder="stroke"
                    stroke={DARTBOARD_COLORS.BLACK}
                    strokeWidth="2"
                    style={{ textShadow: '2px 2px 4px rgba(0,0,0,0.8)' }}
                  >
                    {v}
                  </text>
                );
              })()}
            </g>
          );
        })}

        {/* Outer bull (25) agrandi */}
        <circle
          cx="0"
          cy="0"
          r={R.bullOuter}
          fill={DARTBOARD_COLORS.GREEN}
          stroke={DARTBOARD_COLORS.SPIDER}
          strokeWidth="1"
          filter={hover.ring === "bullOuter" ? "url(#hoverGlow)" : undefined}
          opacity={hover.ring && hover.sector === null ? (hover.ring === "bullOuter" ? 1 : DIM_OPACITY) : 1}
        />
        
        {/* Inner bull (50) agrandi */}
        <circle
          cx="0"
          cy="0"
          r={R.bullInner}
          fill={DARTBOARD_COLORS.RED}
          stroke={DARTBOARD_COLORS.SPIDER}
          strokeWidth="1"
          filter={hover.ring === "bullInner" ? "url(#hoverGlow)" : undefined}
          opacity={hover.ring && hover.sector === null ? (hover.ring === "bullInner" ? 1 : DIM_OPACITY) : 1}
        />

        {/* Suggestions de checkout avec animation */}
        {(() => {
          if (!currentPlayer) return null;
          
          const checkout = getCheckoutSuggestion(
            currentPlayer.score,
            3 - state.dartIndex,
            state.options.outType
          );
          
          return checkout?.map((thr, i) => {
            const { x, y, label } = getThrowPosition(thr);
            return (
              <g key={`checkout-${i}`} opacity={0.9}>
                {/* Halo pulsant */}
                <circle
                  cx={x}
                  cy={y}
                  r="15"
                  fill="url(#checkoutGradient)"
                >
                  <animate
                    attributeName="r"
                    values="15;20;15"
                    dur="1.5s"
                    repeatCount="indefinite"
                  />
                  <animate
                    attributeName="opacity"
                    values="0.7;0.3;0.7"
                    dur="1.5s"
                    repeatCount="indefinite"
                  />
                </circle>
                
                {/* Point de suggestion */}
                <circle
                  cx={x}
                  cy={y}
                  r="8"
                  fill="#3b82f6"
                  stroke="#1d4ed8"
                  strokeWidth="2"
                  filter="url(#hoverGlow)"
                />
                
                {/* Label de suggestion */}
                <text
                  x={x}
                  y={y + 3}
                  fill="white"
                  fontWeight="bold"
                  fontSize="8"
                  textAnchor="middle"
                  style={{ pointerEvents: "none" }}
                >
                  {label}
                </text>
              </g>
            );
          });
        })()}
      </svg>
    </div>
  );
}