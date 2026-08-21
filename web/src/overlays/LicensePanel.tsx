import { useState } from "react";
import { getNativeFunction } from "../bridge/juce";
import { useHostStore } from "../store/hostStore";
import { footerLicenseLabel } from "../chrome/footerLicense";
import { LICENSE_ACTIVATE_URL, licenseBuyerVisible, licenseStatusLine } from "./licenseModel";

export function LicensePanel() {
  const licensed = useHostStore((s) => s.licensed);
  const remain = useHostStore((s) => s.demoRemainSec);
  const email = useHostStore((s) => s.licenseEmail);
  const systemId = useHostStore((s) => s.systemId);
  const err = useHostStore((s) => s.licenseError);
  const [key, setKey] = useState("");
  const [mail, setMail] = useState("");
  const [msg, setMsg] = useState("");
  const [hot, setHot] = useState(false);
  const buyer = licenseBuyerVisible(email);

  const copyId = async () => {
    try {
      await navigator.clipboard.writeText(systemId);
      setMsg("System ID copied");
    } catch {
      setMsg("Copy failed");
    }
  };

  const activate = async () => {
    setMsg("");
    try {
      const res = await fetch(LICENSE_ACTIVATE_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ key: key.trim(), email: mail.trim(), systemId }),
      });
      if (! res.ok) {
        setMsg("Online activate failed — use INSTALL .LIC");
        return;
      }
      setMsg("License stored. Restart the editor if the status stays demo.");
    } catch {
      setMsg("No network — drop a .lic file or INSTALL .LIC");
    }
  };

  const takeFile = (file: File | undefined) => {
    if (! file) {
      return;
    }
    const path = (file as File & { path?: string }).path;
    if (path) {
      void getNativeFunction("pickFile")({ kind: "license", path });
      return;
    }
    void getNativeFunction("pickFile")({ kind: "license" });
  };

  return (
    <div className="flex flex-col gap-4 text-[13px]">
      <section className="border border-accent/40 p-3">
        <div className="text-[11px] tracking-widest text-muted">STATUS</div>
        <div className={`mt-1 text-[18px] ${licensed ? "text-cyan" : "text-accent"}`}>
          {licenseStatusLine(licensed, remain)}
        </div>
        <div className="text-[12px] text-muted">{footerLicenseLabel(licensed, remain)}</div>
        {buyer ? (
          <div className="mt-3 font-brand text-[22px] tracking-wide text-ink">{buyer}</div>
        ) : (
          <div className="mt-3 text-muted">No buyer email on this machine.</div>
        )}
      </section>

      <section className="border border-panel p-3">
        <div className="text-[11px] tracking-widest text-muted">HARDWARE FINGERPRINT</div>
        <div className="mt-2 flex gap-2">
          <input
            readOnly
            className="h-9 min-w-0 flex-1 border border-panel bg-black px-2 font-mono text-[12px] text-cyan"
            value={systemId || "—"}
          />
          <button type="button" className="nk-clip px-3" onClick={() => void copyId()}>Copy</button>
        </div>
      </section>

      <section className="border border-panel p-3">
        <div className="text-[11px] tracking-widest text-muted">ONLINE</div>
        <div className="mt-2 grid grid-cols-2 gap-2">
          <input
            className="h-9 border border-panel bg-black px-2 text-ink"
            placeholder="License key"
            value={key}
            onChange={(e) => setKey(e.target.value)}
          />
          <input
            className="h-9 border border-panel bg-black px-2 text-ink"
            placeholder="Buyer email"
            value={mail}
            onChange={(e) => setMail(e.target.value)}
          />
        </div>
        <button type="button" className="nk-clip mt-2" onClick={() => void activate()}>Activate</button>
      </section>

      <section
        className={`border border-dashed p-4 text-center ${hot ? "border-cyan text-cyan" : "border-panel text-muted"}`}
        onDragOver={(e) => {
          e.preventDefault();
          setHot(true);
        }}
        onDragLeave={() => setHot(false)}
        onDrop={(e) => {
          e.preventDefault();
          setHot(false);
          takeFile(e.dataTransfer.files[0]);
        }}
      >
        <div className="text-[11px] tracking-widest">OFFLINE .LIC</div>
        <p className="mt-2">Drop a signed license file here, or</p>
        <button
          type="button"
          className="nk-clip mt-2"
          onClick={() => void getNativeFunction("pickFile")({ kind: "license" })}
        >
          Install .lic
        </button>
      </section>

      {msg || err ? <div className="text-accent">{msg || err}</div> : null}
    </div>
  );
}
