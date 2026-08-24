/// <reference types="vite/client" />

declare module "*.txt?raw" {
  const content: string;
  export default content;
}

declare module "*.tsx?raw" {
  const content: string;
  export default content;
}

declare module "*?worker" {
  const WorkerFactory: {
    new (): Worker;
  };
  export default WorkerFactory;
}
