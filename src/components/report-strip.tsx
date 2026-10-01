import { hexBytes } from "@/lib/bridge";

export function ReportStrip({ report, label }: { report: Uint8Array; label: string }) {
  return (
    <div className="overflow-hidden rounded-lg border border-border bg-surface">
      <div className="flex items-center justify-between border-b border-border px-4 py-2">
        <span className="text-xs font-medium text-fg-muted">{label}</span>
        <span className="font-mono text-[11px] text-fg-subtle">{report.length} bytes · HID interrupt</span>
      </div>
      <pre className="overflow-x-auto px-4 py-3 font-mono text-[11px] leading-relaxed text-fg-muted">
        {hexBytes(report, report.length)}
      </pre>
    </div>
  );
}
