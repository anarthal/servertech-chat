import { RefObject, useCallback, useEffect, useRef } from "react";
import SendIcon from "@mui/icons-material/Send";

// Code shared between the enter and the icon click interactions
const maybeSendMessage = (
  inputRef: RefObject<HTMLInputElement>,
  onMessage: (msg: string) => void,
) => {
  const messageContent = inputRef.current?.value;
  if (messageContent) {
    onMessage(messageContent);
    inputRef.current.value = "";
  }
};

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
        maybeSendMessage(inputRef, onMessage);
      }
    },
    [onMessage],
  );

  const onSendIconClick = useCallback(() => {
    maybeSendMessage(inputRef, onMessage);
  }, [onMessage]);

  useEffect(() => {
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [onKeyDown]);

  return (
    <div
      className="flex"
      style={{ backgroundColor: "var(--boost-light-grey)" }}
    >
      <div className="flex-1 flex p-2">
        <input
          type="text"
          className="flex-1 text-xl pl-4 pr-4 pt-2 pb-2 border-0 rounded-xl bg-white"
          placeholder="Type a message..."
          ref={inputRef}
        />
      </div>
      <div
        onClick={onSendIconClick}
        className="flex flex-col justify-center pr-4 md:pr-6 pl-2 md:pl-4 cursor-pointer"
      >
        <SendIcon />
      </div>
    </div>
  );
}
