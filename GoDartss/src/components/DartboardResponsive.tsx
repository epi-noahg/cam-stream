"use client";

import { useEffect, useRef, useState } from "react";
import Dartboard from "./Dartboard";

/**
 * Composant Dartboard responsive avec calcul dynamique de taille
 * S'adapte automatiquement à l'espace disponible et aux différents niveaux de zoom
 */
export default function DartboardResponsive() {
  const containerRef = useRef<HTMLDivElement>(null);
  const [size, setSize] = useState(400);

  useEffect(() => {
    const updateSize = () => {
      if (!containerRef.current) return;
      
      const container = containerRef.current;
      const rect = container.getBoundingClientRect();
      
      // Détection mobile/desktop
      const isMobile = window.innerWidth < 768;
      
      // Calcule la taille optimale basée sur l'espace disponible
      const availableSpace = Math.min(rect.width, rect.height);
      const percentage = isMobile ? 0.85 : 0.95; // Plus petit sur mobile pour laisser de la place
      const optimalSize = availableSpace * percentage;
      
      // Taille minimale plus grande sur mobile pour une meilleure utilisabilité
      const minSize = isMobile ? 300 : 200;
      
      setSize(Math.max(optimalSize, minSize));
    };

    // Calcul initial
    updateSize();
    
    // Observer les changements de taille
    const resizeObserver = new ResizeObserver(updateSize);
    if (containerRef.current) {
      resizeObserver.observe(containerRef.current);
    }
    
    // Écouter les changements de zoom/fenêtre
    window.addEventListener('resize', updateSize);
    
    return () => {
      resizeObserver.disconnect();
      window.removeEventListener('resize', updateSize);
    };
  }, []);

  return (
    <div 
      ref={containerRef} 
      className="w-full h-full flex items-center justify-center"
      data-testid="dartboard"
    >
      <div style={{ width: size, height: size }}>
        <Dartboard size={size} />
      </div>
    </div>
  );
}