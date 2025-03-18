import { useState, useLayoutEffect } from "react";

const useIsSmallScreen = (): boolean => {
  const [matches, setMatches] = useState<boolean>(false);

  useLayoutEffect(() => {
    // Tailwind "md" breakpoint
    const media = matchMedia("(width < 48rem)");
    if (media.matches !== matches) {
      setMatches(media.matches);
    }

    // Listen to changes
    const listener = () => setMatches(media.matches);
    media.addEventListener("change", listener);

    // Remove the event listener when unmounting
    return () => media.removeEventListener("change", listener);
  }, [matches]);

  return matches;
};

export default useIsSmallScreen;
