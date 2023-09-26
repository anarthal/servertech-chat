import { useCallback, useState } from "react";

export const DontClearLoading = "dont_clear";

export type InitFn = () => Promise<typeof DontClearLoading | void>;

export default function useLoading(initial: boolean = false) {
  const [loading, setLoading] = useState(initial);
  const launch = useCallback(
    async (fn: InitFn) => {
      setLoading(true);
      try {
        const ret = await fn();
        if (ret !== DontClearLoading) setLoading(false);
      } catch {
        setLoading(false);
      }
    },
    [setLoading],
  );
  return { loading, launch };
}
