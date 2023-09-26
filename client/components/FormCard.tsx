import { PropsWithChildren } from "react";

export default function FormCard({
  children,
  title,
}: PropsWithChildren<{ title: string }>) {
  return (
    <div
      className="bg-white rounded-2xl p-7 flex flex-col"
      style={{ minWidth: "50%" }}
    >
      <p className="text-center text-4xl p-3 m-0">{title}</p>
      <div className="flex justify-center">
        <div
          className="pt-8 pr-5 pl-5 pb-3 flex-1 flex"
          style={{ maxWidth: "40em" }}
        >
          {children}
        </div>
      </div>
    </div>
  );
}
