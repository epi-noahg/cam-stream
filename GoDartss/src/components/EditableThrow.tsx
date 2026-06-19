"use client";

import { useState } from "react";
import { Throw } from "@/types/game";
import { cn } from "@/lib/theme";
import ThrowPad from "@/components/live/ThrowPad";

interface EditableThrowProps {
  throw_: Throw;
  onEdit: (newThrow: Throw) => void;
  isEditable?: boolean;
}

/**
 * Affiche un lancer ; au tap, ouvre le gros pavé tactile (ThrowPad) pour le
 * corriger — adapté à une tablette (anciennement de petits champs inline).
 */
export default function EditableThrow({ throw_, onEdit, isEditable = true }: EditableThrowProps) {
  const [isEditing, setIsEditing] = useState(false);

  const formatThrow = (t: Throw) => {
    if (t.bust === true) return "BUST";
    if (t.value === 0) return "OUT";
    if (t.value === 25 || t.value === 50) return t.value.toString();
    return `${t.value}×${t.multiplier}`;
  };

  if (!isEditable) {
    return <span>{formatThrow(throw_)}</span>;
  }

  return (
    <>
      <span
        onClick={() => setIsEditing(true)}
        className={cn(
          "cursor-pointer px-2 py-1 rounded transition-colors min-h-9 inline-flex items-center",
          "hover:bg-gray-700"
        )}
        title="Toucher pour corriger"
      >
        {formatThrow(throw_)}
      </span>

      {isEditing && (
        <ThrowPad
          title="Corriger la fléchette"
          initialValue={throw_.value}
          initialMultiplier={throw_.multiplier}
          onCancel={() => setIsEditing(false)}
          onSubmit={(value, multiplier) => {
            onEdit({ value, multiplier: multiplier as 1 | 2 | 3, bust: throw_.bust });
            setIsEditing(false);
          }}
        />
      )}
    </>
  );
}
