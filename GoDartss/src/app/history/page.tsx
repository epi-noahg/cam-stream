"use client";

import Link from "next/link";
import { ArrowLeft, Clock, Play } from "lucide-react";
import { theme, cn } from "@/lib/theme";
import { GameHistory } from "@/components/GameHistory";

export default function HistoryPage() {
  return (
    <div className={cn("min-h-screen", theme.bg.primary)}>
      <main className="container mx-auto max-w-4xl py-10 px-4">
        
        {/* Header avec breadcrumb */}
        <div className="space-y-4 mb-8">
          <Link 
            href="/" 
            className={cn(
              "inline-flex items-center gap-2 text-sm font-medium transition-colors",
              theme.text.secondary,
              "hover:text-red-600"
            )}
          >
            <ArrowLeft className="w-4 h-4" />
            Retour au dashboard
          </Link>

          <div className="text-center space-y-2">
            <div className="flex justify-center mb-4">
              <div className={cn(
                "p-3 rounded-full",
                theme.bg.accent
              )}>
                <Clock className={cn("w-8 h-8", theme.text.primary)} />
              </div>
            </div>
            <h1 className={cn(
              "text-4xl font-black",
              theme.text.primary
            )}>
              Historique des parties
            </h1>
            <p className={cn(
              "text-lg max-w-2xl mx-auto",
              theme.text.secondary
            )}>
              Consultez toutes vos parties passées et leurs statistiques
            </p>
          </div>
        </div>

        {/* Historique complet */}
        <div className="space-y-6">
          <GameHistory limit={50} />
        </div>
        
      </main>
    </div>
  );
}