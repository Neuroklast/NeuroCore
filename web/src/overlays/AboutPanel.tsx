import { ABOUT, aboutUserFields } from "./aboutModel";

export function AboutPanel() {
  return (
    <div className="flex flex-col gap-5 text-[13px] leading-6">
      <div className="flex flex-wrap items-center justify-center gap-8 border border-accent/40 bg-black px-4 py-5">
        <a href={ABOUT.website} target="_blank" rel="noreferrer" className="block no-underline">
          <img
            src={ABOUT.logos.manufacturer}
            alt={ABOUT.manufacturerDisplay}
            className="nk-bloom h-16 w-auto"
          />
        </a>
        <img
          src={ABOUT.logos.product}
          alt={ABOUT.product}
          className="nk-bloom h-14 w-auto max-w-[360px] object-contain"
        />
      </div>

      <div className="font-brand text-[18px] tracking-wide text-accent">{ABOUT.product}</div>

      <table className="w-full border-collapse text-[12px]">
        <tbody>
          {aboutUserFields().map((r) => (
            <tr key={r.key} className="border-b border-accent/20">
              <td className="w-28 py-1 text-muted">{r.key}</td>
              <td className="py-1 text-ink">
                {r.key === "Contact" ? (
                  <a href={`mailto:${r.value}`} className="text-accent">{r.value}</a>
                ) : r.value}
              </td>
            </tr>
          ))}
        </tbody>
      </table>

      <p className="text-[12px] text-muted">{ABOUT.copyright}</p>
    </div>
  );
}
