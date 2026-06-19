"use client";

// Ancien flux local retiré — tout passe par le jeu serveur (/live).
import { useEffect } from "react";
import { useRouter } from "next/navigation";

export default function X01SetupRedirect() {
  const router = useRouter();
  useEffect(() => {
    router.replace("/live");
  }, [router]);
  return null;
}
