import { useCallback, useEffect, useRef } from "react";

// The input bar where the user types messages
export default function MessageInputBar({
  onMessage,
}: {
  onMessage: (msg: string) => void;
}) {
  const inputRef = useRef<HTMLInputElement>(null);

  const onKeyDown = useCallback(
    (event: KeyboardEvent) => {
      // Focus the input when the user presses any key, to make interaction smoother
      inputRef.current.focus();

      // Invoke the passed callback when Entr is hit, then clear the message
      if (event.key === "Enter") {
        const messageContent = inputRef.current?.value;
        if (messageContent) {
          onMessage(messageContent);
          inputRef.current.value = "";
        }
      }
    },
    [onMessage],
  );

  useEffect(() => {
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [onKeyDown]);

  return (
    <div className="flex">
      <div
        className="flex-1 flex p-2"
        style={{ backgroundColor: "var(--boost-light-grey)" }}
      >
        <input
          type="text"
          className="flex-1 text-xl pl-4 pr-4 pt-2 pb-2 border-0 rounded-xl"
          placeholder="Type a message..."
          ref={inputRef}
        />
      </div>
    </div>
  );
}
