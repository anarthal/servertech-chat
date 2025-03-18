import React from "react";
import Image from "next/image";
import boostLogo from "@/public/boost.jpg";
import { ArrowBack, GitHub } from "@mui/icons-material";
import useIsSmallScreen from "@/hooks/useIsSmallScreen";

// The common Header with the Boost logo shown in all pages

const links = [
  { text: "Source code", href: "https://github.com/anarthal/servertech-chat" },
  { text: "Docs", href: "https://anarthal.github.io/servertech-chat/" },
];

const BoostLogo = ({ height }: { height: number }) => {
  return (
    <a href="https://www.boost.org/">
      <Image src={boostLogo} height={height} alt="Boost logo"></Image>
    </a>
  );
};

// Header for large screens
const LargeHeader = () => {
  return (
    <div className="flex m-3">
      <BoostLogo height={60} />
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
};

// Header for small screens
const SmallHeader = ({
  showArrow,
  onArrowClick,
}: {
  showArrow: boolean;
  onArrowClick?: () => void;
}) => {
  return (
    <div className="flex m-3">
      {showArrow && (
        <div
          onClick={onArrowClick}
          className="flex flex-col justify-center pr-2"
        >
          <ArrowBack
            color={onArrowClick === undefined ? "disabled" : undefined}
          />
        </div>
      )}
      <div className="flex flex-1 justify-between">
        <BoostLogo height={40} />
        <a href="https://github.com/anarthal/servertech-chat" className="pl-2">
          <GitHub style={{ width: "40px", height: "40px" }} />
        </a>
      </div>
    </div>
  );
};

export default function Header({
  showArrow,
  onArrowClick = undefined,
}: {
  showArrow: boolean;
  onArrowClick?: () => void;
}) {
  const isSmallScreen = useIsSmallScreen();
  return (
    <>
      {isSmallScreen ? (
        <SmallHeader showArrow={showArrow} onArrowClick={onArrowClick} />
      ) : (
        <LargeHeader />
      )}
    </>
  );
}
