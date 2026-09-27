import React from "react";
import ReactDOM from "react-dom/client";
import { App } from "./app/App";
import { setSplashProgress } from "./theme/splash";
import "./theme/tailwind.css";

setSplashProgress(40);

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
