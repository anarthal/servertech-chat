import React from "react";
import Image from "next/image";
import boostLogo from "@/public/boost.jpg";

// The common Header with the Boost logo shown in all pages

const links = [
  { text: "Source code", href: "https://github.com/anarthal/servertech-chat" },
  { text: "Docs", href: "https://anarthal.github.io/servertech-chat/" },
];

export default function Header() {
  return (
    <div className="flex m-3">
      <Image src={boostLogo} height={60} alt="Boost logo"></Image>
      <div className="flex-1 flex justify-end align-middle">
        {links.map(({ text, href }) => (
          <div key={href} className="flex flex-col justify-center pr-12 pl-12">
            <a className="no-underline text-2xl text-black" href={href}>
              {text}
            </a>
          </div>
        ))}
      </div>
    </div>
  );
}
