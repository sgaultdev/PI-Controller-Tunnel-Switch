import type { ReactNode } from "react";
import { Link, useRouterState } from "@tanstack/react-router";
import { Cable, Cpu, Download, GitBranch } from "lucide-react";
import { cn } from "@/lib/utils";

const NAV = [
  { to: "/", label: "Workbench", icon: Cable },
  { to: "/setup", label: "Wiring", icon: Cpu },
  { to: "/protocol", label: "Protocol", icon: GitBranch },
  { to: "/install", label: "Install", icon: Download },
] as const;

export function Shell({ children }: { children: ReactNode }) {
  const pathname = useRouterState({ select: (s) => s.location.pathname });

  return (
    <div className="min-h-dvh bg-bg text-fg">
      <div className="mx-auto flex min-h-dvh max-w-[1400px] flex-col lg:flex-row">
        <header className="flex items-center justify-between border-b border-border px-5 py-4 lg:hidden">
          <Brand />
        </header>

        <aside className="hidden w-[220px] shrink-0 flex-col border-r border-border px-4 py-8 lg:flex">
          <Brand />
          <nav className="mt-10 flex flex-col gap-1">
            {NAV.map((item) => (
              <NavLink key={item.to} {...item} active={pathname === item.to} />
            ))}
          </nav>
          <p className="mt-auto pt-8 text-xs leading-relaxed text-fg-subtle">
            Native USB daemon for Raspberry Pi and Windows. Mixed pads in, Pro hub or GameCube adapter out.
          </p>
        </aside>

        <div className="flex min-w-0 flex-1 flex-col pb-20 lg:pb-0">
          <main className="flex-1 px-5 py-6 sm:px-8 sm:py-8">{children}</main>
        </div>

        <nav className="fixed inset-x-0 bottom-0 z-20 grid grid-cols-4 border-t border-border bg-bg/95 pb-[env(safe-area-inset-bottom)] backdrop-blur lg:hidden">
          {NAV.map((item) => (
            <Link
              key={item.to}
              to={item.to}
              className={cn(
                "flex min-h-14 flex-col items-center justify-center gap-1 text-[11px] font-medium",
                pathname === item.to ? "text-fg" : "text-fg-muted",
              )}
            >
              <item.icon className="size-4" strokeWidth={1.75} />
              {item.label}
            </Link>
          ))}
        </nav>
      </div>
    </div>
  );
}

function Brand() {
  return (
    <Link to="/" className="flex items-center gap-2.5">
      <span className="flex size-8 items-center justify-center rounded-sm border border-border bg-surface">
        <Cable className="size-4 text-wire" strokeWidth={1.75} />
      </span>
      <span className="text-[15px] font-semibold tracking-tight">Railbridge</span>
    </Link>
  );
}

function NavLink({
  to,
  label,
  icon: Icon,
  active,
}: {
  to: string;
  label: string;
  icon: typeof Cable;
  active: boolean;
}) {
  return (
    <Link
      to={to}
      className={cn(
        "flex h-10 items-center gap-2.5 rounded-md px-3 text-sm transition-colors duration-150",
        active ? "bg-surface-2 text-fg" : "text-fg-muted hover:bg-surface hover:text-fg",
      )}
    >
      <Icon className="size-4" strokeWidth={1.75} />
      {label}
    </Link>
  );
}
