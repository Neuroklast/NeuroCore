import { nk } from "../theme/tokens";

export interface LegalSection {
  id: string;
  title: string;
  body: string;
}

export interface ThirdPartyCredit {
  name: string;
  holder: string;
  note: string;
}

export const ABOUT = {
  product: nk.product,
  productCode: "NeuroKore",
  version: nk.version,
  manufacturer: "NEUROKLAST",
  manufacturerDisplay: nk.company,
  website: "https://neuroklast.net",
  email: "info@neuroklast.net",
  manufacturerCode: "NRKL",
  pluginCode: "NRKO",
  formats: ["Standalone", "VST3", "AU"] as const,
  category: "Fx | Distortion",
  copyright: "Copyright (c) 2024–2026 NEUROKLAST. All rights reserved.",
  blurb:
    "NEUROKORE is an insert or send effect by Neuroklast. Load a factory sound, turn knobs a–f, or write a short formula.",
  logos: {
    manufacturer: "./img/neuroklast.png",
    product: "./img/neurokore.png",
  },
  eula: [
    {
      id: "license",
      title: "1. License",
      body:
        "Subject to a valid license issued by NEUROKLAST, you may install and use the Software on computers you own or control, for your own music production. One license is bound as described in the license file delivered with your purchase. You may make one backup copy of the installer.",
    },
    {
      id: "restrictions",
      title: "2. Restrictions",
      body:
        "You may not copy, modify, distribute, sublicense, sell, rent, lease, reverse engineer, decompile, or create derivative works of the Software, except as expressly allowed by a separate written agreement with NEUROKLAST or by applicable law that cannot be waived.",
    },
    {
      id: "ownership",
      title: "3. Ownership",
      body:
        "The Software is licensed, not sold. NEUROKLAST retains all right, title, and interest in the Software. Third-party components (including JUCE and the VST3 SDK) remain the property of their respective owners and are used under their own licenses.",
    },
    {
      id: "termination",
      title: "4. Termination",
      body:
        "This license ends if you breach this Agreement. On termination you must stop using the Software and destroy all copies in your possession.",
    },
    {
      id: "disclaimer",
      title: "5. Disclaimer",
      body:
        "THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.",
    },
    {
      id: "liability",
      title: "6. Limitation of liability",
      body:
        "IN NO EVENT SHALL NEUROKLAST BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY ARISING FROM THE SOFTWARE OR ITS USE, INCLUDING LOST PROFITS, LOST DATA, OR DAMAGE TO AUDIO EQUIPMENT, TO THE MAXIMUM EXTENT PERMITTED BY LAW.",
    },
    {
      id: "contact",
      title: "7. Contact",
      body: "https://neuroklast.net — By installing or using the Software you agree to this Agreement.",
    },
  ] satisfies LegalSection[],
  thirdParty: [
    {
      name: "JUCE",
      holder: "Raw Material Software Limited",
      note: "Used under the JUCE licence applicable to this product.",
    },
    {
      name: "VST3 SDK",
      holder: "Steinberg Media Technologies GmbH",
      note: "VST is a trademark of Steinberg Media Technologies GmbH.",
    },
  ] satisfies ThirdPartyCredit[],
} as const;

export function aboutLegalIds(): string[] {
  return ABOUT.eula.map((s) => s.id);
}

/** Settings ABOUT opens the About overlay, not License. */
export function settingsAboutTarget(): "about" {
  return "about";
}

export function aboutUserFields(): Array<{ key: string; value: string }> {
  return [
    { key: "Version", value: ABOUT.version },
    { key: "Formats", value: ABOUT.formats.join(" · ") },
    { key: "Web", value: ABOUT.website },
    { key: "Mail", value: ABOUT.email },
  ];
}
