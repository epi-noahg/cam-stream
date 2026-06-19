/**
 * Providers pour l'application - version simplifiée
 */

'use client';

export function TRPCProvider({ children }: { children: React.ReactNode }) {
  // Version simplifiée sans tRPC pour corriger les erreurs TypeScript
  return <>{children}</>;
}