import { twMerge } from "tailwind-merge";

/**
 * Fusionne proprement plusieurs classes Tailwind.
 * Ex. cn("p-4", condition && "bg-red-500")
 */
export function cn(...classes: (string | undefined | null | false)[]) {
  return twMerge(classes.filter(Boolean).join(" "));
}
