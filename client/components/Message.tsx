// A message in the chat

import NameAvatar from "@/components/NameAvatar";

function formatDate(date: number): string {
  return new Intl.DateTimeFormat("en-US", {
    dateStyle: "medium",
    timeStyle: "short",
  }).format(new Date(date));
}

// A message in the conversation screen, sent by a user that's not ourselves
export const OtherUserMessage = ({
  username,
  content,
  timestamp,
}: {
  username: string;
  content: string;
  timestamp: number;
}) => {
  return (
    <div className="flex flex-row pt-3 pb-3">
      <div className="pr-5 flex flex-col justify-end">
        <NameAvatar name={username} />
        <p className="m-0 pt-1 text-sm">{username}</p>
      </div>
      <div className="flex-[6] bg-white rounded-lg pt-4 pl-5 pr-2">
        <p className="m-0">{content}</p>
        <p
          className="m-0 pb-2 pr-2 text-xs text-right"
          style={{ color: "var(--boost-grey)" }}
        >
          {formatDate(timestamp)}
        </p>
      </div>
      <div className="flex-[3]"></div>
    </div>
  );
};

// A message in the conversation screen, sent by ourselves
export const MyMessage = ({
  content,
  timestamp,
}: {
  content: string;
  timestamp: number;
}) => {
  return (
    <div className="flex flex-row pt-3 pb-3">
      <div className="flex-[4]"></div>
      <div className="flex-[6] bg-green-100 rounded-lg pt-4 pl-5 pr-2">
        <p className="m-0">{content}</p>
        <p
          className="m-0 pb-2 pr-2 text-xs text-right"
          style={{ color: "var(--boost-grey)" }}
        >
          {formatDate(timestamp)}
        </p>
      </div>
    </div>
  );
};
